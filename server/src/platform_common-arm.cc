/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015,2017,2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#include "platform-arm.h"
#include "support.h"

#include <l4/util/kip.h>
#include <assert.h>

void Platform_arm::setup_kernel_config_arm_common(l4_kernel_info_t *kip)
{
  const char *s = l4_kip_version_string(kip);
  assert(s);

  kernel_type = EL_Support::EL1;
  l4util_kip_for_each_feature(s)
  if (!strcmp(s, "arm:hyp"))
    {
      kernel_type = EL_Support::EL2;
      break;
    }

  // Ensure later stages do not overwrite the CPU boot-up code
  extern char cpu_bootup_code_start[], cpu_bootup_code_end[];
  mem_manager->regions->add(Region::n(cpu_bootup_code_start,
                                      cpu_bootup_code_end,
                                      ".cpu_boot", Region::Root), true);
}

