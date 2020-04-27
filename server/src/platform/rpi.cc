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
    unsigned long m;

#ifdef ARCH_arm
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
    switch ((m >> 4) & 0xf00)
      {
      case 0xc00: rpi_ver = 2; break;
      case 0xd00: rpi_ver = ((m >> 4) & 0xf) == 3 ? 3 : 4; break;
      default: rpi_ver = 1; break;
      }
#endif
#ifdef ARCH_arm64
    asm volatile("mrs %0, midr_el1" : "=r" (m));
    if ((m & 0xff0ffff0) == 0x410fd030)
      rpi_ver = 3;
    else
      rpi_ver = 4;
#endif

    switch (rpi_ver)
      {
      case 1:
        _base = 0x20000000;
        kuart.base_address = _base + 0x00201000;
        break;
      case 2:
        _base = 0x3f000000;
        kuart.base_address = _base + 0x00201000;
        break;
      case 3:
        _base = 0x3f000000;
        kuart.base_address = _base + 0x00215040;
        kuart.irqno        = 29;
        kuart.base_baud    = 31250000;
        break;
      case 4:
        _base = 0xfe000000;
        kuart.base_address = _base + 0x00215040;
        kuart.irqno        = 32 + 93; // GIC
        kuart.base_baud    = 62500000;
        break;
      };

    kuart.baud = 115200;

    if (rpi_ver == 1 || rpi_ver == 2)
      {
        kuart.base_baud  = 0;
        kuart.irqno      = 57;
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart_flags       |=   L4_kernel_options::F_uart_base
                             | L4_kernel_options::F_uart_baud
                             | L4_kernel_options::F_uart_irq;
        static L4::Io_register_block_mmio r(kuart.base_address);
        static L4::Uart_pl011 _uart(kuart.base_baud);
        _uart.startup(&r);
        set_stdio_uart(&_uart);
      }
    else
      {
        kuart.reg_shift = 2;
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart_flags       |=   L4_kernel_options::F_uart_base
                             | L4_kernel_options::F_uart_baud
                             | L4_kernel_options::F_uart_irq;
        static L4::Uart_16550 _uart(kuart.base_baud, 0, 8);
        setup_16550_mmio_uart(&_uart);
      }
  }

  void post_memory_hook()
  {
    mem_manager->regions->add(Region::n(0x0, 0x1000, ".mpspin",
                                        Region::Arch, 0));
  }

  void reboot()
  {
    enum { Rstc = 0x1c, Wdog = 0x24 };

    L4::Io_register_block_mmio r(_base + 0x00100000);

    l4_uint32_t pw = 0x5a << 24;
    r.write(Wdog, pw | 8);
    r.write(Rstc, (r.read<l4_uint32_t>(Rstc) & ~0x30) | pw | 0x20);

    while (1)
      ;
  }
private:
  l4_addr_t _base;
};
}

REGISTER_PLATFORM(Platform_arm_rpi);
