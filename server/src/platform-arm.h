/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015,2017,2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include "platform.h"
#include "koptions-def.h"
#include "arch/arm/mem.h"

#include <l4/sys/kip.h>

class Platform_arm : public Platform_base
{
public:
  enum class EL_Support { EL2, EL1, Unknown };

  static void reboot_psci()
  {
    register unsigned long r0 asm("r0") = 0x84000009;
    asm volatile(
#ifdef ARCH_arm
                 ".arch armv7-a\n"
                 ".arch_extension sec\n"
#endif
                 "smc #0" : : "r" (r0));
  }

  void setup_kernel_config(l4_kernel_info_t *kip) override;

  void setup_kernel_options(L4_kernel_options::Options *lko) override
  {
#if defined(ARCH_arm64) // disabled on arm32 until assembler support lands
    // If we do not get an spin address from DT, all cores might start
    // at the same time and are caught by bootstrap
    extern l4_umword_t mp_launch_spin_addr;
    asm volatile("" : : : "memory");
    Barrier::dmb_cores();
    lko->core_spin_addr = (l4_uint64_t)&mp_launch_spin_addr;
#else
    (void)lko;
#endif // defined(ARCH_arm64)
  }


  void setup_kernel_config_arm_common(l4_kernel_info_t *kip);

  virtual void module_load_hook(l4_addr_t addr, l4_umword_t file_sz,
                                l4_umword_t mem_sz,
                                char const* cmdline) override;

private:
  EL_Support kernel_type = EL_Support::Unknown;

  virtual bool arm_switch_to_hyp() { return false; }
};



