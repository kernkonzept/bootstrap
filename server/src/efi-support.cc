/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *            Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_base.h>
#include <l4/sys/types.h>
#include <l4/util/mb_info.h>
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

l4util_l4mod_info *Efi::construct_mbi(l4util_l4mod_info *mbi)
{
  if (!_video_fb_size)
    return mbi;

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

void Efi::setup_gop()
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
          Print((CHAR16 *)L"%c%d: %dx%d ",
                memcmp(info, gop->Mode->Info, sizeof(*info)) == 0 ? '*' : ' ',
                i,
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

Efi efi;
