/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2018-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 */

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

void Platform_arm::module_load_hook(l4_addr_t addr, l4_umword_t file_sz,
                                    l4_umword_t, char const*)
{
  l4_uint32_t* end = (l4_uint32_t *)(addr + file_sz);
  for (l4_uint32_t *cp = (l4_uint32_t *)addr; cp < end; ++cp)
    if (*cp == 0xd4000001 && kernel_type == EL_Support::EL2) // svc #0
      {
        printf("WARNING: Kernel with virtualization support does not match userland without!\n"
               "         booting might fail silently or with a kernel panic\n"
               "         please adapt your kernel or userland config if needed\n");
        break; // There's only a single syscall insn in the binary
      }
    else if (*cp == 0xd4000002 && kernel_type == EL_Support::EL1) // hvc #0
      {
        printf("WARNING: Kernel without virtualization support does not match userland with it!\n"
               "         booting might fail silently or with a kernel panic\n"
               "         please adapt your kernel or userland config if needed\n");
        break; // There's only a single syscall insn in the binary
      }
}
