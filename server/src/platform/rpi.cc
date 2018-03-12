/*!
 * \file   rpi.cc
 * \brief  Support for the Raspberry Pi
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
#include <l4/drivers/uart_pl011.h>

namespace {

class Platform_arm_rpi : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    static L4::Io_register_block_mmio r(0x20201000);
    static L4::Uart_pl011 _uart(3000000);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_rpi);
