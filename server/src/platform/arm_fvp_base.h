/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *            Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */
/**
 * \file
 * \brief Common definition for ARM's AEM FVP Base and BaseR platforms
 */

#pragma once

#include <assert.h>
#include <l4/drivers/uart_pl011.h>

#include "platform-arm.h"
#include "support.h"
#include "startup.h"

namespace {

template<bool SWAP>
struct Platform_arm_fvp_base_common : public Platform_arm,
                                      public Boot_modules_image_mode
{
  enum
  {
    Ram_base_low  = SWAP ?        0x0U : 0x80000000U,
    Ram_base_high = 0x880000000U,
    Periph_base   = SWAP ? 0x80000000U :        0x0U,
  };

  bool probe() override { return true; }

  void init() override
  {
    kuart.base_baud    = 24019200;
    kuart.base_address = Periph_base + 0x1c090000;
    kuart.baud         = 115200;
    kuart.irqno        = 37;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    // FVP is a virtual platform, so be flexible wrt our memory
    // configuration
    unsigned size_mb = RAM_SIZE_MB;
    if (sizeof(unsigned long) == 4 && size_mb > 2048)
      size_mb = 2048;

    mem_manager->ram->add(
      Region::start_size(Ram_base_low,
                         ((size_mb < 2048 ? size_mb : 2048) << 20),
                         ".ram", Region::Ram));
    if (size_mb > 2048)
      mem_manager->ram->add(Region::start_size(Ram_base_high,
                                               ((size_mb - 2048) << 20),
                                               ".ram", Region::Ram));
  }
};

}
