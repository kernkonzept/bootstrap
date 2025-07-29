/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "init_kip.h"

#include <l4/drivers/of_if.h>

void
init_kip_v2_arch(l4_kernel_info_t* l4i)
{
  L4_drivers::Of_if of_if;
  //l4i->total_ram = of_if.detect_ramsize();
  printf("TBD: set total RAM via mem-descs!\n");
  l4i->frequency_cpu = (l4_uint32_t)of_if.detect_cpu_freq() / 1000; //kHz

  of_if.vesa_set_mode(0x117);
}
