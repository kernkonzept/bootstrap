/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "platform.h"
#include "support.h"
#include "startup.h"

struct boot_args boot_args;

extern "C" void __main(unsigned long hartid, unsigned long fdt);
void __main(unsigned long hartid, unsigned long fdt)
{
  clear_bss();

  boot_args.r[0] = hartid;
  boot_args.r[1] = fdt;

  ctor_init();

  Platform_base::iterate_platforms();
  printf("Boot hart ID: %lu\n", hartid);

  init_modules_infos();
  startup(mod_header->mbi_cmdline());
  l4_infinite_loop();
}
