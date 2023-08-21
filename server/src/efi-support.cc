/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *            Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_base.h>
#include "efi-support.h"
#include "panic.h"

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/irq.h> // l4util_cli
#endif

#ifdef ARCH_arm64
extern "C" void armv8_disable_mmu(void);
#endif

namespace {

class Efi_console : public L4::Uart
{
  CHAR16 mutable _last_char = 0;

public:
  bool startup(L4::Io_register_block const *) override { return true; }
  void shutdown() override {}
  bool change_mode(Transfer_mode, Baud_rate) override { return true; }

  int get_char(bool blocking = true) const override
  {
    while (char_avail() <= 0)
      if (!blocking)
        return -1;

    int ret = _last_char;
    _last_char = 0;
    return ret;
  }

  int char_avail() const override
  {
    EFI_SYSTEM_TABLE *st = efi.system_table();

    while (!_last_char)
      {
        EFI_INPUT_KEY key;
        EFI_STATUS status = uefi_call_wrapper(st->ConIn->ReadKeyStroke, 2,
                                              st->ConIn, &key);
        if (EFI_ERROR(status))
          {
            _last_char = 0;
            break;
          }

        _last_char = key.UnicodeChar;
      }

    return _last_char != 0;
  }

  int write(char const *s, unsigned long count, bool) const override
  {
    unsigned long len = count;
    EFI_SYSTEM_TABLE *st = efi.system_table();
    CHAR16 str[2];
    str[1] = 0;

    while (count--)
      {
        str[0] = *s++;
        uefi_call_wrapper(st->ConOut->OutputString, 2, st->ConOut, str);
      }

    return len;
  }
};

Efi_console efi_console;

}

void
Efi::init(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  _image = image;
  _sys_table = systab;

  InitializeLib(image, systab);

  Print(L"EFI L4::Bootstrap\n\r");

  // Use EFI console until we've exited the boot services
  set_stdio_uart(&efi_console);

  // Get ACPI RSDP. Will be need in any case for Fiasco and possibly in
  // bootstrap.
  EFI_GUID Acpi20TableGuid = ACPI_20_TABLE_GUID;
  EFI_STATUS r = LibGetSystemConfigurationTable(&Acpi20TableGuid, &_acpi_rsdp);
  if (r != EFI_SUCCESS)
    r = LibGetSystemConfigurationTable(&AcpiTableGuid, &_acpi_rsdp);

  if (r != EFI_SUCCESS)
    {
      _acpi_rsdp = NULL;
      Print(L"No RSDP found in EFI system table\n\r");
    }
}

void
Efi::exit_boot_services()
{
  // Must retrieve memory map again because we possibly made UEFI calls in
  // between. Without this the ExitBootServices() is likely to fail!
  UINTN num_entries, key, desc_size;
  uint32_t desc_ver;
  LibMemoryMap(&num_entries, &key, &desc_size, &desc_ver);

  EFI_STATUS r;
  r = uefi_call_wrapper(_sys_table->BootServices->ExitBootServices, 2,
                        _image, key);
  if (r != EFI_SUCCESS)
    panic("EFI: ExitBootServices failed: %u\n", (unsigned)r);

  // The EFI console is not available any more!
  if (uart() == &efi_console)
    set_stdio_uart(nullptr);

  // UEFI has interrupts enabled as per specification. Fiasco boot protocol
  // requires interrupts to be disabled, though. Overall, ensure all is
  // masked.
#if defined(ARCH_arm64)
  asm volatile("msr DAIFSet, #0xf");
#elif defined(ARCH_x86) || defined(ARCH_amd64)
  l4util_cli();
#endif

#ifdef ARCH_arm64
  // UEFI did setup the MMU with caches. Disable everything because we move
  // code around and will jump to it at the end.
  armv8_disable_mmu();
#endif
}

Region
Efi::new_region(EFI_MEMORY_DESCRIPTOR const *m, char const *name,
                Region::Type type, char sub)
{
  return Region::start_size(m->PhysicalStart, 0x1000 * m->NumberOfPages, name,
                            type, sub);
}

void
Efi::setup_memory()
{
  Region_list *ram = mem_manager->ram;
  Region_list *regions = mem_manager->regions;

  UINTN num_entries, key, desc_size;
  uint32_t desc_ver;
  EFI_MEMORY_DESCRIPTOR *efi_mem_desc = LibMemoryMap(&num_entries, &key,
                                                     &desc_size, &desc_ver);
  if (!efi_mem_desc)
    panic("EFI: failed to get memory map");
  if (desc_ver != EFI_MEMORY_DESCRIPTOR_VERSION)
    panic("EFI: unexpected memory descriptor version: 0x%x\n", desc_ver);

  void *const map_end = (char *)efi_mem_desc + num_entries * desc_size;
  for (char *d = (char *)efi_mem_desc; d < map_end; d += desc_size)
    {
      EFI_MEMORY_DESCRIPTOR *m = (EFI_MEMORY_DESCRIPTOR *)d;

      if (0)
        Print(L"MD%02d: type=%02x v=%llx p=%llx numpages=%llx attr=%llx\n\r",
              (d - (char *)efi_mem_desc) / desc_size,
              m->Type, m->PhysicalStart,
              m->VirtualStart, m->NumberOfPages, m->Attribute);

      switch (m->Type)
        {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
          // Must allow overlaps of RAM regions. This has been observed on
          // Ampere Altra...
          ram->add(new_region(m, ".ram", Region::Ram), true);
          break;
        case EfiACPIReclaimMemory: // memory holds ACPI tables
          regions->add(new_region(m, ".ACPI", Region::Arch, Region::Arch_acpi));
          break;
        case EfiACPIMemoryNVS: // memory reserved by firmware
          regions->add(new_region(m, ".ACPI", Region::Arch, Region::Arch_nvs));
          break;
        }
    }

  // add region for ACPI tables
  if (_acpi_rsdp)
    regions->add(Region::start_size(l4_trunc_page((l4_addr_t)_acpi_rsdp),
                                    L4_PAGESIZE, ".ACPI",
                                    Region::Info, Region::Info_acpi_rsdp),
                 true);

  // merge adjacent regions
  ram->optimize();
}

Efi efi;
