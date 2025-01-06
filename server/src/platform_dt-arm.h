/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015, 2017, 2019-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include "platform-arm.h"
#include "platform_dt.h"
#include "support.h"

class Platform_dt_arm : public Platform_dt<Platform_arm>
{
public:
  l4_addr_t get_fdt_addr() const override
  {
    #if defined(ARCH_arm64)
      return boot_args.r[0];
    #elif defined(ARCH_arm)
      return boot_args.r[2];
    #endif
  }

  void setup_kernel_options(L4_kernel_options::Options *lko) override
  {
    lko->core_spin_addr = dt.have_fdt() ? dt.cpu_release_addr() : -1ULL;
    if (lko->core_spin_addr == -1ULL)
      Platform_dt<Platform_arm>::setup_kernel_options(lko);
  }

protected:
  static int parse_gic_irq(Dt::Node node)
  {
    Dt::Array_prop<3> interrupts = node.get_prop_array("interrupts", { 1, 1, 1 });
    if (!interrupts.is_valid() || interrupts.elements() < 1)
      return -1;

    unsigned gic_type = interrupts.get(0, 0);
    if (gic_type != 0 && gic_type != 1)
      return -1;

    return interrupts.get(0, 1) + (gic_type == 0 ? 32 : 0);
  }
};
