/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2018-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 */

#include <l4/bid_config.h>

#include "panic.h"
#include "platform-arm.h"
#include <assert.h>
#include <stdio.h>

static inline unsigned current_el()
{
  l4_umword_t current_el;
  asm ("mrs %0, CurrentEL" : "=r" (current_el));
  return (current_el >> 2) & 3;
}

void Platform_arm::setup_kernel_config(l4_kernel_info_t *kip)
{
  setup_kernel_config_arm_common(kip);

  assert(kernel_type != EL_Support::Unknown);
  if (kernel_type == EL_Support::EL2 && current_el() < 2)
    panic("Kernel requires EL2 (virtualization) but running in EL1.");
}

