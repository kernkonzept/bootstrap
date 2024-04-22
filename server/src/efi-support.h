/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 */
#pragma once

extern "C" {
#include <efi.h>
#include <efilib.h>
}

#include "support.h"

class Efi
{
public:
  void init(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
  void setup_memory();
  void setup_gop();
  l4util_l4mod_info *construct_mbi(l4util_l4mod_info *mbi);

  void exit_boot_services();

  EFI_SYSTEM_TABLE *system_table() const { return _sys_table; }
  void *acpi_rsdp() const { return _acpi_rsdp; }

private:
  Region new_region(EFI_MEMORY_DESCRIPTOR const *td, char const *name,
                    Region::Type type, char sub = 0);


  EFI_HANDLE _image;
  EFI_SYSTEM_TABLE *_sys_table;
  void *_acpi_rsdp;

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION _video_info;
  EFI_PHYSICAL_ADDRESS                 _video_fb_phys_base;
  unsigned                             _video_fb_size;
};

extern Efi efi;
