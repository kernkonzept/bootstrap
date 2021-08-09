/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

#include "platform_dt.h"

class Platform_riscv_base : public Platform_dt<Platform_base>
{
public:
  bool probe() override { return true; }
  void init() override;
  l4_addr_t get_fdt_addr() const override;
  void init_dt(Internal_module_list &mods) override;
  void setup_kernel_config(l4_kernel_info_t*kip) override;
  void boot_kernel(unsigned long entry) override;

protected:
  virtual void setup_kuart() = 0;
  void setup_kuart_from_dt(char const *compatible);

private:
  bool parse_kuart_node(Dt::Node node, char const *compatible);

  char const *riscv_cpu_isa(Dt::Node cpu) const;
  bool riscv_cpu_hartid(Dt::Node cpu, l4_uint32_t &hartid) const;
  bool parse_isa_ext(l4_kip_platform_info_arch &arch_info) const;
  bool parse_harts(l4_kip_platform_info_arch &arch_info) const;
  l4_uint32_t get_timebase_frequency() const;
  bool parse_interrupt_target_contexts(l4_kip_platform_info_arch &arch_info) const;
};
