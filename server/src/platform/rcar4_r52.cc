/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_sh.h>

#include "platform-arm.h"
#include "startup.h"
#include "support.h"

namespace {

class Platform_arm_rcar4 : public Platform_arm, public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    // Write timer frequency to CNTFRQ. The kernel expects a valid value.
    asm volatile ("MCR p15, 0, %0, c14, c0, 0" : : "r"(16666600));

    kuart.reg_shift    = 0;
    kuart.base_baud    = 14745600;
    kuart.base_address = 0xe6e60000;
    kuart.irqno        = 283;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Uart_sh _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    // There is also the Secure FW in RT-SRAM!
    mem_manager->ram->add(Region(0xeb200000U, 0xeb37ffffU,
                                 ".ram", Region::Ram));
  }
};

}

REGISTER_PLATFORM(Platform_arm_rcar4);
