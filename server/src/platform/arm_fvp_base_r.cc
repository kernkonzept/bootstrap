/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */
/**
 * \file
 * \brief  Support for ARM's AEM FVP BaseR platform
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

  unsigned num_nodes() override
  { return 4; }
};

}

REGISTER_PLATFORM(Platform_arm_fvp_base_r);
