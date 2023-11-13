/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 */

#include <l4/drivers/uart_omap35x.h>
#include "support.h"
#include "startup.h"
#include "platform_dt-arm.h"

namespace {
class Platform_ti_am : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.base_address = 0x02800000;
    kuart.base_baud    = 0;
    kuart.irqno        = 32 + 178;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_omap35x _uart;
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot() override
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_ti_am);
