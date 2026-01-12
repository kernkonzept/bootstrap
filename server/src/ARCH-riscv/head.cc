/*
 * Copyright (C) 2021, 2025 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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
