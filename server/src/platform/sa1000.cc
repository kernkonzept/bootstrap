/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   support_sa1000.cc
 * \brief  Support for SA1000 platform
 */

#include "support.h"
#include "platform-arm.h"
#include "startup.h"
#include <l4/drivers/uart_sa1000.h>

namespace {
class Platform_arm_sa1000 : public Platform_single_region_ram<Platform_arm>
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0x80010000; // 0x80050000
    kuart.base_baud    = 0;
    kuart.baud         = 115200;
    kuart.irqno        = 17;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Uart_sa1000 _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_sa1000);
