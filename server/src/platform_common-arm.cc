/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015,2017,2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#include "platform-arm.h"
#include "support.h"

#include <l4/sys/kip.h>
#include <assert.h>

void Platform_arm::setup_kernel_config_arm_common(l4_kernel_info_t *kip)
{
  kernel_type = l4_kip_kernel_has_feature(kip, "arm:hyp")
                ? EL_Support::EL2 : EL_Support::EL1;

  // Ensure later stages do not overwrite the CPU boot-up code
  extern char cpu_bootup_code_start[] __attribute__((weak));
  extern char cpu_bootup_code_end[] __attribute__((weak));
  if (cpu_bootup_code_start)
    {
      l4_size_t const size = cpu_bootup_code_end - cpu_bootup_code_start;
      mem_manager->regions->add(Region::start_size(cpu_bootup_code_start, size,
                                                   ".cpu_boot", Region::Root),
                                true);
    }
}

