/*
 * Copyright (C) 2013 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file
 * \brief  Support for sunxi platforms
 */

#include "platform-arm.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_sunxi : public Platform_single_region_ram<Platform_arm>
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0x01c28000;
    kuart.reg_shift    = 2;
    kuart.base_baud    = 1500000;
    kuart.baud         = 115200;
    kuart.irqno        = 33;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_sunxi);
