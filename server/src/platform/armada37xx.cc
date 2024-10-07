/*!
 * \file
 * \brief  Support for Marvell Armada 37x0
 *
 * \date   2017
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2017, 2024 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_mvebu.h>

#include "startup.h"
#include "platform_dt-arm.h"
#include "support.h"

namespace {
class Platform_arm_armada37xx : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_baud  = 25804800;
    kuart_flags     |= L4_kernel_options::F_uart_baud;

    dt.check_for_dt();
    dt.get_stdout_uart("marvell,armada-3700-uart", parse_gic_irq,
                       &kuart, &kuart_flags);

    static L4::Uart_mvebu _uart(kuart.base_baud);
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

REGISTER_PLATFORM(Platform_arm_armada37xx);
