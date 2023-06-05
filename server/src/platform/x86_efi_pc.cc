#include "boot_modules.h"
#include "efi-support.h"
#include "platform.h"
#include "support.h"
#include "x86_pc-base.h"
#include <l4/util/mb_info.h>
#include <stdio.h>

namespace {

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

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION _video_info;
  EFI_PHYSICAL_ADDRESS                 _video_fb_phys_base;
  unsigned                             _video_fb_size;

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Boot_modules_image_mode::construct_mbi(mod_addr, mods);

    // we use the copy here instead of reading it out directly because
    // EFI is already exited in setup_memory_map which is called much too
    // early

    unsigned long long vbe_sz = sizeof(l4util_mb_vbe_ctrl_t) + sizeof(l4util_mb_vbe_mode_t);
    l4_addr_t vbe_addr = mem_manager->find_free_ram(vbe_sz);
    if (!vbe_addr)
      {
        printf("EFI: Unable to reserve region to hold VBE config.\n");
        return mbi;
      }

    mem_manager->regions->add(Region::start_size(vbe_addr, vbe_sz, ".vbe",
                                                 Region::Root, L4_FPAGE_RW));

    mbi->vbe_ctrl_info = vbe_addr;
    mbi->vbe_mode_info = vbe_addr + sizeof(l4util_mb_vbe_ctrl_t);

    l4util_mb_vbe_ctrl_t *vbe = (l4util_mb_vbe_ctrl_t *)(l4_addr_t)mbi->vbe_ctrl_info;
    l4util_mb_vbe_mode_t *vbi = (l4util_mb_vbe_mode_t *)(l4_addr_t)mbi->vbe_mode_info;

    // Use the structures phys_base plus the following reserved1 field to
    // hold a 64-bit value
    vbi->phys_base    = _video_fb_phys_base;
    vbi->reserved1    = _video_fb_phys_base >> 32;
    vbi->x_resolution = _video_info.HorizontalResolution;
    vbi->y_resolution = _video_info.VerticalResolution;

    vbe->total_memory = (_video_fb_size + 64 * 1024 - 1) / (64 * 1024); // in 64k chunks

    switch (_video_info.PixelFormat)
      {
      case PixelRedGreenBlueReserved8BitPerColor:
        vbi->red_mask_size = 8;
        vbi->green_mask_size = 8;
        vbi->blue_mask_size = 8;

        vbi->red_field_position = 0;
        vbi->green_field_position = 8;
        vbi->blue_field_position = 16;
        break;

      case PixelBlueGreenRedReserved8BitPerColor:
        vbi->red_mask_size = 8;
        vbi->green_mask_size = 8;
        vbi->blue_mask_size = 8;

        vbi->red_field_position = 16;
        vbi->green_field_position = 8;
        vbi->blue_field_position = 0;
        break;

      case PixelBitMask:

        vbi->red_mask_size
          = __builtin_popcount(_video_info.PixelInformation.RedMask);
        vbi->green_mask_size
          = __builtin_popcount(_video_info.PixelInformation.GreenMask);
        vbi->blue_mask_size
          = __builtin_popcount(_video_info.PixelInformation.BlueMask);

        vbi->red_field_position = ffs(_video_info.PixelInformation.RedMask);
        if (vbi->red_field_position)
          --vbi->red_field_position;

        vbi->green_field_position = ffs(_video_info.PixelInformation.GreenMask);
        if (vbi->green_field_position)
          --vbi->green_field_position;

        vbi->blue_field_position = ffs(_video_info.PixelInformation.BlueMask);
        if (vbi->blue_field_position)
          --vbi->blue_field_position;
        break;

      default:
      case PixelBltOnly:
        return mbi;
      }

    // All format defaults:
    vbi->bytes_per_scanline = _video_info.PixelsPerScanLine * 4;
    vbi->bits_per_pixel = 32;

    mbi->flags |= L4UTIL_MB_VIDEO_INFO;

    return mbi;
  }

  void setup_gop()
  {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID efi_gop = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_STATUS rc = LibLocateProtocol(&efi_gop, (void **)&gop);
    if (EFI_ERROR(rc))
      {
        Print((CHAR16 *)L"No Graphics\n");
        return;
      }

    int i, imax;
    void *r;

    imax = gop->Mode->MaxMode;

    const bool verbose = false;

    if (verbose)
      Print((CHAR16 *)L"GOP reports MaxMode %d\n", imax);
    for (i = 0; i < imax; i++)
      {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN SizeOfInfo;
        r = uefi_call_wrapper((void *)gop->QueryMode, 4, gop, i, &SizeOfInfo,
                               &info);
        if (EFI_ERROR(r) && (EFI_STATUS)r == EFI_NOT_STARTED)
          {
            r = uefi_call_wrapper((void *)gop->SetMode, 2, gop,
                                  gop->Mode->Mode);
            // TODO: check r
            r = uefi_call_wrapper((void *)gop->QueryMode, 4, gop, i,
                                  &SizeOfInfo, &info);
          }

        if (EFI_ERROR(r))
          {
            CHAR16 Buffer[64];
            StatusToString(Buffer, (EFI_STATUS)r);
            Print((CHAR16 *)L"EFI GOP: %d: Bad response from QueryMode: %s (%d)\n",
                  i, Buffer, r);
            continue;
          }

        if (verbose)
          {
            Print((CHAR16 *)L"%c%d: %dx%d ", memcmp(info,gop->Mode->Info,sizeof(*info)) == 0 ? '*' : ' ', i,
                  info->HorizontalResolution,
                  info->VerticalResolution);
            switch (info->PixelFormat)
              {
              case PixelRedGreenBlueReserved8BitPerColor:
                  Print((CHAR16 *)L"RGBR");
                break;
              case PixelBlueGreenRedReserved8BitPerColor:
                  Print((CHAR16 *)L"BGRR");
                break;
              case PixelBitMask:
                Print((CHAR16 *)L"R:%08x G:%08x B:%08x X:%08x",
                      info->PixelInformation.RedMask,
                      info->PixelInformation.GreenMask,
                      info->PixelInformation.BlueMask,
                      info->PixelInformation.ReservedMask);
                break;
              case PixelBltOnly:
                Print((CHAR16 *)L"(blt only)");
                break;
              default:
                Print((CHAR16 *)L"(Invalid pixel format)");
                break;
              }
            Print((CHAR16 *)L" pitch %d\n", info->PixelsPerScanLine);
          }

        if (0 == memcmp(info, gop->Mode->Info, sizeof(*info)))
          {
            _video_info         = *info;
            _video_fb_phys_base = gop->Mode->FrameBufferBase;
            _video_fb_size      = gop->Mode->FrameBufferSize;

            if (verbose)
              Print((CHAR16 *)L" base=%x  size=%x\n",
                    _video_fb_phys_base, _video_fb_size);
          }
    }
  }

  void exit_boot_services() override
  {
    efi.exit_boot_services();
  }
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


  _x86_pc_platform.setup_gop();

  Platform_base::platform = &_x86_pc_platform;
  _x86_pc_platform.init();
  _x86_pc_platform.setup_uart(efi_cmdline);

  init_modules_infos();

  _x86_pc_platform.disable_pci_bus_master();

  startup(mod_info_mbi_cmdline(mod_header));

  return EFI_SUCCESS;
}
