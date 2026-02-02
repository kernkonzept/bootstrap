/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Copyright (C) 2015, 2017, 2019-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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

  void query_psci_method()
  {
    Dt::Node psci = dt.node_by_compatible("arm,psci-1.0");
    if (!psci.is_valid())
      psci = dt.node_by_compatible("arm,psci-0.2");
    if (!psci.is_valid())
      psci = dt.node_by_compatible("arm,psci");

    set_psci_method(Psci_unsupported);

    if (psci.is_valid())
      {
        const char *method = psci.get_prop_str("method");
        if (method && !strcmp(method, "smc"))
          set_psci_method(Psci_smc);
        else if (method && !strcmp(method, "hvc"))
          set_psci_method(Psci_hvc);
      }
  }

  void set_dtb_in_kip(l4_kernel_info_t *kip)
  {
    kip->dt_addr = reinterpret_cast<l4_umword_t>(dt.fdt());
  }
};
