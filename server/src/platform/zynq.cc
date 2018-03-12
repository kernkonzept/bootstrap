/*!
 * \file
 * \brief  Support for Zynq platforms
 *
 * \date   2013
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2013 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include <l4/drivers/uart_cadence.h>

namespace {
class Platform_arm_zynq : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    static L4::Uart_cadence _uart;
    unsigned long uart_addr;
    switch (PLATFORM_UART_NR) {
      case 0: uart_addr = 0xe0000000; // qemu
      default:
      case 1: uart_addr = 0xe0001000; // zedboard
    };
    static L4::Io_register_block_mmio r(uart_addr);
    _uart.startup(&r);
    _uart.change_mode(3, 115200);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
    L4::Io_register_block_mmio r(0xf8000200);
    r.write<unsigned>(1, 0);
  }
};
}

REGISTER_PLATFORM(Platform_arm_zynq);
