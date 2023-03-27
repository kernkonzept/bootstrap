/*!
 * \file
 * \brief  Support for Renesas Gen4
 *
 * \date   2022
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2022 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_sh.h>
#include "support.h"
#include "startup.h"
#include "platform_dt-arm.h"

namespace {
class Platform_arm_rcar4 : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.reg_shift    = 0;
    kuart.base_baud    = 14745600;
    kuart.base_address = 0xe6e60000; // SCIF0
    kuart.irqno        = 283; // SCIF0

    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Uart_sh _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot() override
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_arm_rcar4);
