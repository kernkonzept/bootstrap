/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2014-2021 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */
#include "efi-support.h"
#include "platform.h"
#include "startup.h"
#include "support.h"

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  ctor_init();
  efi.init(image, systab);

  // Do the usual platform iteration as done by head.cc.
  Platform_base::iterate_platforms();
  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));

  return EFI_SUCCESS;
}
