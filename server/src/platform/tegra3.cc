/*
 * Copyright (C) 2013 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file
 * \brief  Support for Tegra 3 platforms
 */

#include "support.h"
#include "platform-arm.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_tegra3 : public Platform_single_region_ram<Platform_arm>
{
  bool probe() override { return true; }

  void init() override
  {
    switch (PLATFORM_UART_NR)
      {
      case 0:
        kuart.base_address = 0x70006000;
        kuart.irqno        = 68;
        break;
      default:
      case 2:
        kuart.base_address = 0x70006200;
        kuart.irqno        = 78;
        break;
      };
    kuart.reg_shift    = 2;
    kuart.base_baud    = 25459200;
    kuart.baud         = 115200;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_tegra3);
