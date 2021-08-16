/*!
 * \file
 * \brief  Support for NXP LX2160
 *
 * \date   2020
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2020 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"

namespace {

class Platform_arm_lx2160 : public Platform_dt
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0x21c0000;
    kuart.base_baud    = 0;
    kuart.irqno        = 64;
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
    reboot_psci();
    while (1)
      ;
  }
};
}

REGISTER_PLATFORM(Platform_arm_lx2160);
