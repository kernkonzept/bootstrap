/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2014-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */

#include "panic.h"
#include "platform-arm.h"
#include <assert.h>
#include <stdio.h>

static inline l4_umword_t
running_in_hyp_mode()
{
  l4_umword_t cpsr;
  asm volatile("mrs %0, cpsr" : "=r"(cpsr));
  return (cpsr & 0x1f) == 0x1a;
}

void
Platform_arm::setup_kernel_config(l4_kernel_info_t *kip)
{
  setup_kernel_config_arm_common(kip);
  l4_kip_platform_info_arch *ia = &kip->platform_info.arch;

  asm("mrc p15, 0, %0, c0, c0, 0" : "=r" (ia->cpuinfo.MIDR));
  asm("mrc p15, 0, %0, c0, c0, 1" : "=r" (ia->cpuinfo.CTR));
  asm("mrc p15, 0, %0, c0, c0, 2" : "=r" (ia->cpuinfo.TCMTR));
#ifdef CONFIG_MMU
  asm("mrc p15, 0, %0, c0, c0, 3" : "=r" (ia->cpuinfo.TLBTR));
#endif
  asm("mrc p15, 0, %0, c0, c0, 5" : "=r" (ia->cpuinfo.MPIDR));
  asm("mrc p15, 0, %0, c0, c0, 6" : "=r" (ia->cpuinfo.REVIDR));

  if (((ia->cpuinfo.MIDR >> 16) & 0xf) >= 7)
    {
      asm("mrc p15, 0, %0, c0, c1, 0" : "=r" (ia->cpuinfo.ID_PFR[0]));
      asm("mrc p15, 0, %0, c0, c1, 1" : "=r" (ia->cpuinfo.ID_PFR[1]));
      asm("mrc p15, 0, %0, c0, c1, 2" : "=r" (ia->cpuinfo.ID_DFR0));
      asm("mrc p15, 0, %0, c0, c1, 3" : "=r" (ia->cpuinfo.ID_AFR0));
      asm("mrc p15, 0, %0, c0, c1, 4" : "=r" (ia->cpuinfo.ID_MMFR[0]));
      asm("mrc p15, 0, %0, c0, c1, 5" : "=r" (ia->cpuinfo.ID_MMFR[1]));
      asm("mrc p15, 0, %0, c0, c1, 6" : "=r" (ia->cpuinfo.ID_MMFR[2]));
      asm("mrc p15, 0, %0, c0, c1, 7" : "=r" (ia->cpuinfo.ID_MMFR[3]));
      asm("mrc p15, 0, %0, c0, c2, 0" : "=r" (ia->cpuinfo.ID_ISAR[0]));
      asm("mrc p15, 0, %0, c0, c2, 1" : "=r" (ia->cpuinfo.ID_ISAR[1]));
      asm("mrc p15, 0, %0, c0, c2, 2" : "=r" (ia->cpuinfo.ID_ISAR[2]));
      asm("mrc p15, 0, %0, c0, c2, 3" : "=r" (ia->cpuinfo.ID_ISAR[3]));
      asm("mrc p15, 0, %0, c0, c2, 4" : "=r" (ia->cpuinfo.ID_ISAR[4]));
      asm("mrc p15, 0, %0, c0, c2, 5" : "=r" (ia->cpuinfo.ID_ISAR[5]));
    }

  assert(kernel_type != EL_Support::Unknown);
  if (kernel_type == EL_Support::EL2 && !running_in_hyp_mode())
    {
      printf("  Detected HYP kernel, switching to HYP mode\n");

      if (   ((ia->cpuinfo.MIDR >> 16) & 0xf) != 0xf // ARMv7
          || (((ia->cpuinfo.ID_PFR[1] >> 12) & 0xf) == 0)) // No Virt Ext
        panic("\nCPU does not support Virtualization Extensions\n");

      if (!arm_switch_to_hyp())
        panic("\nNo switching functionality available on this platform.\n");
      if (!running_in_hyp_mode())
        panic("\nFailed to switch to HYP as required by Fiasco.OC.\n");
    }

  if (kernel_type == EL_Support::EL1 && running_in_hyp_mode())
    {
      printf("  Non-HYP kernel detected but running in HYP mode, switching back.\n");
      asm volatile("mov r3, lr                    \n"
                   "mcr p15, 0, sp, c13, c0, 2    \n"
                   "mrs r0, cpsr                  \n"
                   "bic r0, #0x1f                 \n"
                   "orr r0, #0x13                 \n"
                   "orr r0, #0x100                \n"
                   "adr r1, 1f                    \n"
                   ".inst 0xe16ff000              \n" // msr spsr_cfsx, r0
                   ".inst 0xe12ef301              \n" // msr elr_hyp, r1
                   ".inst 0xe160006e              \n" // eret
                   "nop                           \n"
                   "1: mrc p15, 0, sp, c13, c0, 2 \n"
                   "mov lr, r3                    \n"
                   : : : "r0", "r1" , "r3", "lr", "memory");
    }
}

