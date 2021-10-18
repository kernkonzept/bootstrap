/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"
#include "dt.h"

extern char _start;

namespace {
class Platform_arm_virt : public Platform_base, public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    // set defaults for reg_shift and baud_rate
    kuart.baud      = 115200;
    kuart.reg_shift = 0;

    kuart.base_address = 0x09000000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 33;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);

    /* This is a QEMU special. QEMU copies the FDT to the start of the
     * RAM but does not provide the address in case the payload is
     * started as ELF.
     * Further, Fiasco wants to have this part of the RAM, so copy the FDT
     * somewhere else. */
    void *fdt = (void *)RAM_BASE;
    if (fdt_check_header(fdt) == 0)
      {
        unsigned s = fdt_totalsize(fdt);
        unsigned long dst = l4_trunc_page((unsigned long)&_start - s);
        memmove((void *)dst, fdt, s);
        boot_args.r[0] = dst;
      }
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    dt.check_for_dt();
    dt.setup_memory();
  }

  void reboot()
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_arm_virt);
