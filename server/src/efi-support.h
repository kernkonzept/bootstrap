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

  void exit_boot_services();

  EFI_SYSTEM_TABLE *system_table() const { return _sys_table; }
  void *acpi_rsdp() const { return _acpi_rsdp; }

private:
  Region new_region(EFI_MEMORY_DESCRIPTOR const *td, char const *name,
                    Region::Type type, char sub = 0);


  EFI_HANDLE _image;
  EFI_SYSTEM_TABLE *_sys_table;
  void *_acpi_rsdp;
};

extern Efi efi;
