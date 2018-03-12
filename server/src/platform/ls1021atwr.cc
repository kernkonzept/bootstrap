/*!
 * \file
 * \brief  Support for Freescale TWR-LS1021A
 *
 * \date   2015
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2015 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_ls1021atwr : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    switch (PLATFORM_UART_NR)
      {
      case 0:
        kuart.base_address = 0x21c0500;
        break;
      case 1:
        kuart.base_address = 0x21c0600;
        break;
      };
    kuart.base_baud    = 9375000;
    kuart.baud         = 115200;
    kuart.irqno        = 118;
    kuart.reg_shift    = 0;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_ls1021atwr);
