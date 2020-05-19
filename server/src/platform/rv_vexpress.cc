/*!
 * \file
 * \brief  Support for the rv platform
 *
 * \date   2011
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2011 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "startup.h"

#include <l4/drivers/uart_pl011.h>

namespace {

class Platform_arm_rv_vexpress : public Platform_single_region_ram
{
  bool probe() { return true; }
  void init()
  {
    kuart.base_baud    = 24019200;
    kuart.baud         = 115200;
    kuart.irqno        = 37;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

#if defined ARCH_arm
    kuart.base_address = 0x10009000;
    unsigned long m;
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
    if ((m & 0x00000070) == 0x70)
      kuart.base_address = 0x1c090000;
#else
    kuart.base_address = 0x1c090000;
#endif

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};

}

REGISTER_PLATFORM(Platform_arm_rv_vexpress);
