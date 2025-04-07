/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file   support_om.cc
 * \brief  Support for the OpenMoko platform
 */

#include "support.h"
#include "startup.h"
#include "platform-arm.h"

#include <l4/drivers/uart_s3c2410.h>
#include <l4/drivers/uart_dummy.h>

namespace {
class Platform_arm_om : public Platform_single_region_ram<Platform_arm>
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0x50000000;
    kuart.baud = 115200;
    kuart.irqno = 28;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Uart_s3c2410 _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_om);
