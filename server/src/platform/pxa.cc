/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   support_pxa.cc
 * \brief  Support for the PXA platform
 */

#include "mmio_16550.h"
#include "support.h"
#include "platform-arm.h"

namespace {
class Platform_arm_pxa : public Platform_single_region_ram<Platform_arm>
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0x40100000;
    kuart.reg_shift    = 2;
    kuart.base_baud    = L4::Uart_16550::Base_rate_pxa;
    kuart.baud         = 115200;
    kuart.irqno        = -1;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 1 << 6, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_pxa);
