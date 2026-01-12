/*
 * Copyright (C) 2021, 2024-2025 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_irq;

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
