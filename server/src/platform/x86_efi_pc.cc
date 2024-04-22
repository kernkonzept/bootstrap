#include "boot_modules.h"
#include "efi-support.h"
#include "platform.h"
#include "support.h"
#include "x86_pc-base.h"
#include <stdio.h>

namespace {

// This "Uart" uses the EFI services for screen printouts
class Uart_efi : public L4::Uart
{
public:
  void disable()
  {
    _enabled = false;
  }

private:
  bool startup(L4::Io_register_block const *) override
  {
    return true;
  }

  int write(char const *s, unsigned long count, bool) const override
  {
    CHAR16 buf[2];
    buf[1] = 0;

    if (!_enabled)
      return count;

    for (unsigned long i = 0; i < count; ++i)
      {
        buf[0] = s[i];
        Output(buf);
      }

    return count;
  }

  void shutdown() override {}
  bool change_mode(Transfer_mode, Baud_rate) override { return true; }
  int get_char(bool) const override { return -1; }
  int char_avail() const override { return false; } // TODO input?

  bool _enabled = true;
};

class Platform_x86_efi : public Platform_x86,
  public Boot_modules_image_mode
{
public:
  enum
  {
    Max_cmdline_length = 1024,
  };

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    efi.setup_memory();
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Boot_modules_image_mode::construct_mbi(mod_addr, mods);
    return efi.construct_mbi(mbi);
  }

  void exit_boot_services() override
  {
    _efi_uart.disable();
    efi.exit_boot_services();
  }

  Uart_efi _efi_uart;
};

Platform_x86_efi _x86_pc_platform;
}

static char efi_cmdline[Platform_x86_efi::Max_cmdline_length + 1];

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  efi.init(image, systab);
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


  efi.setup_gop();

  Platform_base::platform = &_x86_pc_platform;
  _x86_pc_platform.init();
  _x86_pc_platform.setup_uart(efi_cmdline, &_x86_pc_platform._efi_uart);

  init_modules_infos();

  _x86_pc_platform.disable_pci_bus_master();

  startup(mod_header->mbi_cmdline());

  return EFI_SUCCESS;
}
