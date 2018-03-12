/*!
 * \file   support_imx.cc
 * \brief  Support for the i.MX platform
 *
 * \date   2008-02-04
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_imx.h>
#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"


namespace {
class Platform_arm_imx : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    // set defaults for reg_shift and baud_rate
    kuart.baud      = 115200;
    kuart.reg_shift = 0;

#ifdef PLATFORM_TYPE_imx21
    kuart.base_address = 0x1000A000;
    kuart.irqno        = 20;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_imx21 _uart;
#elif defined(PLATFORM_TYPE_imx28)
    kuart.base_address = 0x80074000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 47;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_imx35)
    static L4::Uart_imx35 _uart;
    switch (PLATFORM_UART_NR) {
      default:
      case 1: kuart.base_address = 0x43f90000;
              kuart.irqno        = 45;
              break;
      case 2: kuart.base_address = 0x43f94000;
              kuart.irqno        = 32;
              break;
      case 3: kuart.base_address = 0x5000c000;
              kuart.irqno        = 18;
              break;
    }
    static L4::Io_register_block_mmio r(kuart.base_address);
#elif defined(PLATFORM_TYPE_imx51)
    kuart.base_address = 0x73fbc000;
    kuart.irqno = 31;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_imx51 _uart;
#elif defined(PLATFORM_TYPE_imx53)
    kuart.base_address = 0x53fbc000;
    kuart.irqno = 31;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_imx51 _uart;
#elif defined(PLATFORM_TYPE_imx6) || defined(PLATFORM_TYPE_imx6ul)
    switch (PLATFORM_UART_NR) {
      case 1: kuart.base_address = 0x02020000;
              kuart.irqno        = 58;
              break;
      default:
      case 2: kuart.base_address = 0x021e8000;
              kuart.irqno        = 59;
              break;
      case 3: kuart.base_address = 0x021ec000;
              kuart.irqno        = 60;
              break;
      case 4: kuart.base_address = 0x021f0000;
              kuart.irqno        = 61;
              break;
      case 5: kuart.base_address = 0x021f4000;
              kuart.irqno        = 62;
              break;
    };
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_imx6 _uart;
#else
#error Which platform type?
#endif
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_imx);
