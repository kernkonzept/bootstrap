/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "platform_riscv.h"
#include "startup.h"

namespace {
class Platform_riscv_migv : public Platform_riscv_base
{
  void setup_kuart() override
  {
    // Set defaults, can be overwritten by device tree.
    kuart.baud         = 115200;
    kuart.reg_shift    = 2;
    kuart.base_address = 0x404000;
    kuart.base_baud    = 14976000; // 115200 * 0x81, taken from ROM bootloader
    kuart.irqno        = 9;

    setup_kuart_from_dt("ns16550a");
  }

  void reboot() override
  {
    printf("RISC-V MiG-V reboot not implemented\n");

    l4_infinite_loop();
  }
};
}

REGISTER_PLATFORM(Platform_riscv_migv);
