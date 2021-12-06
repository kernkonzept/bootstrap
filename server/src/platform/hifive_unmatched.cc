/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "platform_riscv.h"
#include "startup.h"

namespace {
class Platform_riscv_hifive_unmatched : public Platform_riscv_base
{
  void setup_kuart() override
  {
    // Set defaults, can be overwritten by device tree.
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.base_address = 0x10010000;
    // Device tree does not specify a concrete clock-frequency but refers to the
    // Periphery Clock (pclk) on the Power Reset Clocking Interrupt (PRCI)
    // block. To figure out the pclk frequency configured we would need to
    // implement a fu740-c000-prci specific mechanism/driver.
    // So for now hard-code the value which was obtained by multiplying the UART
    // divisor, configured by U-Boot, with baud rate: 1128 * 115200 = 130000000
    kuart.base_baud    = 130000000;
    kuart.irqno        = 3;

    setup_kuart_from_dt("sifive,uart0");
  }

  void reboot() override
  {
    printf("HiFive Unmatched reboot not implemented\n");

    l4_infinite_loop();
  }
};
}

REGISTER_PLATFORM(Platform_riscv_hifive_unmatched);
