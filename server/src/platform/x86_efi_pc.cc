#include "support.h"
#include "x86_pc-base.h"
#include <string.h>

#include "startup.h"
#include "panic.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include <efi.h>
#include <efilib.h>
}
#include <l4/util/irq.h> // l4util_cli

namespace {

static inline Region
new_region(EFI_MEMORY_DESCRIPTOR const *td, char const *name,
           Region::Type type, char sub = 0)
{
  return Region::n(td->PhysicalStart,
                   td->PhysicalStart + (0x1000 * td->NumberOfPages),
                   name, type, sub);
}

class Platform_x86_efi : public Platform_x86,
  public Boot_modules_image_mode
{
public:
  enum
  {
    Max_cmdline_length = 1024,
  };

  Boot_modules *modules() { return this; }

  void init_efi(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
  {
    efi_image = image;
    efi_systab = systab;

    InitializeLib(efi_image, efi_systab);
  }

  void setup_memory_map()
  {
    UINTN num_entries;
    UINTN key;
    UINTN desc_size;
    uint32_t desc_ver;

    EFI_MEMORY_DESCRIPTOR *efi_mem_desc
      = LibMemoryMap(&num_entries, &key, &desc_size, &desc_ver);

    void *table;
    EFI_GUID Acpi20TableGuid = ACPI_20_TABLE_GUID;
    EFI_STATUS exit_status
      = LibGetSystemConfigurationTable(&Acpi20TableGuid, &table);

    if (exit_status != EFI_SUCCESS)
        exit_status = LibGetSystemConfigurationTable(&AcpiTableGuid, &table);

    if (exit_status != EFI_SUCCESS)
      printf("No RSDP found in EFI system table\n");

    exit_status = (EFI_STATUS)uefi_call_wrapper((void*)(BS->ExitBootServices), 2, efi_image, key);
    if (exit_status != EFI_SUCCESS)
      printf("EFI, ExitBootServices failed: %u\n", (unsigned)exit_status);
    else
      printf("Successfully exited EFI boot services.\n");

    // UEFI may have enabled interrupts because of loaded device drivers
    // Fiasco boot protocol requires interrupts to be disabled
    l4util_cli();

    Region_list *ram = mem_manager->ram;
    Region_list *regions = mem_manager->regions;

    void *const map_end = (char *)efi_mem_desc + num_entries * desc_size;
    for (char *d = (char *)efi_mem_desc; d < map_end; d += desc_size)
      {
        EFI_MEMORY_DESCRIPTOR *td = (EFI_MEMORY_DESCRIPTOR *)d;

        switch (td->Type)
          {
          case EfiLoaderCode:
          case EfiLoaderData:
          case EfiBootServicesCode:
          case EfiBootServicesData:
          case EfiConventionalMemory:
            ram->add(new_region(td, ".ram", Region::Ram));
            break;
          case EfiACPIReclaimMemory: // memory holds ACPI tables
            regions->add(new_region(td, ".ACPI", Region::Arch,
                                    Region::Arch_acpi));
            break;
          case EfiACPIMemoryNVS: // memory reserved by firmware
            regions->add(new_region(td, ".ACPI", Region::Arch,
                                    Region::Arch_nvs));
            break;
          }
      }

    // add region for ACPI tables
    regions->add(Region::n(l4_trunc_page((l4_addr_t)table),
                           l4_trunc_page((l4_addr_t)table) + L4_PAGESIZE,
                           ".ACPI", Region::Info, Region::Info_acpi_rsdp),
                 true);

    // merge adjacent regions
    ram->optimize();
  }

private:
  EFI_HANDLE efi_image;
  EFI_SYSTEM_TABLE *efi_systab;
};

Platform_x86_efi _x86_pc_platform;
}

static char efi_cmdline[Platform_x86_efi::Max_cmdline_length + 1];

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  _x86_pc_platform.init_efi(image, systab);
  ctor_init();

  // Get the optional load options and interpret it as the command line.
  // This is currently used for setting up the UART.
  void *loaded_image = nullptr;
  EFI_STATUS rc = (EFI_STATUS)uefi_call_wrapper(BS->OpenProtocol, 6, image,
                                                &LoadedImageProtocol,
                                                &loaded_image,
                                                image,
                                                NULL,
                                                EFI_OPEN_PROTOCOL_GET_PROTOCOL);

  if (!EFI_ERROR(rc))
    {
      UINT32 length = ((EFI_LOADED_IMAGE *)loaded_image)->LoadOptionsSize / 2;
      CHAR16 *buffer =
        (CHAR16 *)((EFI_LOADED_IMAGE *)loaded_image)->LoadOptions;

      if (length > Platform_x86_efi::Max_cmdline_length)
        length = Platform_x86_efi::Max_cmdline_length;

      if (buffer == nullptr)
        length = 0;

      // The load options are in UTF-16, thus we trivially convert the usable
      // characters to ASCII. Proper Unicode support would be an overkill for
      // the current purpose.
      for (UINT32 i = 0; i < length; ++i)
        {
          if (buffer[i] == L'\0')
            {
              efi_cmdline[i] = '\0';
              break;
            }

          if (buffer[i] <= 255)
            efi_cmdline[i] = buffer[i];
          else
            efi_cmdline[i] = '?';
        }

      // Make sure the command line is always null-terminated.
      efi_cmdline[length] = '\0';
    }
  else
    efi_cmdline[0] = '\0';

  Platform_base::platform = &_x86_pc_platform;
  _x86_pc_platform.init();
  _x86_pc_platform.setup_uart(efi_cmdline);

  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));

  return EFI_SUCCESS;
}
