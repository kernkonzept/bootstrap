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

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "mmio_16550.h"

namespace {

class Platform_arm_rpi : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    unsigned rpi_ver;

#ifdef ARCH_arm
    unsigned long m;
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
    switch ((m >> 4) & 0xf00)
      {
      case 0xc00: rpi_ver = 2; break;
      case 0xd00: rpi_ver = 3; break;
      default: rpi_ver = 1; break;
      }
#endif
#ifdef ARCH_arm64
    rpi_ver = 3;
#endif

    switch (rpi_ver)
      {
      case 1:
        kuart.base_address = 0x20201000;
        break;
      case 2:
        kuart.base_address = 0x3f201000;
        break;
      case 3:
        kuart.base_address = 0x3f215040;
        break;
      };

    kuart.baud         = 115200;

    if (rpi_ver == 1 || rpi_ver == 2)
      {
        kuart.irqno        = 57;

        static L4::Io_register_block_mmio r(kuart.base_address);
        static L4::Uart_pl011 _uart(3000000);
        _uart.startup(&r);
        set_stdio_uart(&_uart);
      }
    else
      {
        kuart.base_baud    = 31250000;
        kuart.irqno        = 29;
        kuart.reg_shift    = 2;

        static L4::Uart_16550 _uart(kuart.base_baud, 0, 8);
        setup_16550_mmio_uart(&_uart);
      }
  }
};
}

REGISTER_PLATFORM(Platform_arm_rpi);
