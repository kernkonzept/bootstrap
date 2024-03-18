/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "arm_fvp_base.h"

namespace {

class Platform_arm_fvp_base_r : public Platform_arm_fvp_base_common<true>
{
  using Base = Platform_arm_fvp_base_common<true>;

  void init() override
  {
    // Write timer frequency to CNTFRQ. The kernel expects a valid value.
#ifdef ARCH_arm64
    asm volatile ("MSR CNTFRQ_EL0, %0" : : "r"(100000000));
#else
    asm volatile ("MCR p15, 0, %0, c14, c0, 0" : : "r"(100000000));
#endif

    Base::init();
  }

  // Only the 32-bit variant is AMP. On 64-bit it's an SMP system.
#ifdef ARCH_arm
  unsigned num_nodes() override
  { return 4; }

  unsigned current_node() override
  {
    unsigned long mpidr;
    asm ("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return mpidr & 0xffU;
  }
#endif

  void setup_kernel_options(L4_kernel_options::Options *lko) override
  {
    // next uart
    lko->uart.irqno += lko->node;
    lko->uart.base_address += 0x10000UL * lko->node;
    Base::setup_kernel_options(lko);
  }
};

}

REGISTER_PLATFORM(Platform_arm_fvp_base_r);
