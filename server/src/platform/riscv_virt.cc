/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "platform_riscv.h"
#include "qemu_fw_cfg.h"
#include "qemu_ramfb.h"
#include "startup.h"

namespace {
class Platform_riscv_virt : public Platform_riscv_base
{
  void setup_kuart() override
  {
    // Set defaults, can be overwritten by device tree.
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.base_address = 0x10000000;
    kuart.base_baud    = 1843200;
    kuart.irqno        = 10;

    setup_kuart_from_dt("ns16550a");
  }

  void setup_fw_cfg()
  {
    Dt::Node fw_cfg = dt.node_by_compatible("qemu,fw-cfg-mmio");
    l4_uint64_t fw_cfg_addr;
    if (fw_cfg.is_valid() && fw_cfg.get_reg(0, &fw_cfg_addr))
      Fw_cfg::init_mmio(fw_cfg_addr);
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Platform_riscv_base::construct_mbi(mod_addr, mods);
    setup_fw_cfg();
    setup_ramfb(mbi);
    return mbi;
  }

  void reboot() override
  {
    printf("RISC-V Virtual Platform reboot not implemented\n");

    l4_infinite_loop();
  }
};
}

REGISTER_PLATFORM(Platform_riscv_virt);
