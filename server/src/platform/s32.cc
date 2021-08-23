/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_linflex.h>
#include "support.h"
#include "startup.h"


namespace {
class Platform_s32 : public Platform_dt
{
  bool probe() { return true; }

  void init()
  {
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.base_address = 0x401c8000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 32 + 82;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_linflex _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
    if (0)
      reboot_psci();
    else
      {
        *(volatile unsigned char *)0x40078018 = 0xf;

        *(volatile unsigned *)0x40088004 = 2;
        *(volatile unsigned *)0x40088008 = 1;
        *(volatile unsigned *)0x40088000 = 0x5AF0;
        *(volatile unsigned *)0x40088000 = 0xA50F;
      }
  }
};
}

REGISTER_PLATFORM(Platform_s32);
