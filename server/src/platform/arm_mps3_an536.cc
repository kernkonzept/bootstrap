/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 * \brief  Support for ARM's MPS3 AN536 platform
 */

#include <assert.h>
#include <l4/drivers/uart_apb.h>

#include "platform-arm.h"
#include "support.h"
#include "startup.h"

namespace {

class Platform_arm_mps3_an536 : public Platform_arm,
                                public Boot_modules_image_mode
{
  enum : l4_addr_t
  {
    Ram_base             = 0x2000'0000,
    Ram_size             = RAM_SIZE_MB * 1024ul * 1024ul,
    Max_ram_size_mb      = 3072, // 3 GiB

    Shared_periph_base   = 0xe000'0000,
    Private_periph_base  = 0xe7c0'0000,
  };

  static_assert(Ram_base == RAM_BASE, "Unexpected RAM base.");
  static_assert(RAM_SIZE_MB <= Max_ram_size_mb, "Unexpected RAM size.");

  bool probe() override { return true; }

  void init() override
  {
    // TODO: Use private peripheral UARTs (UART0 and UART1) instead?
    //       (they use PPI interrupt)
    kuart.base_baud    = 50'000'000; // PERIPH_CLK: 50MHz
    // UART2 for node 0 and UART3 for node 1
    kuart.base_address = Shared_periph_base + 0x205000;
    kuart.baud         = 115200;
    kuart.irqno        = 13;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_apb _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() override { return this; }

  unsigned num_nodes() override
  { return 2; }

  void setup_kernel_options(L4_kernel_options::Options *lko) override
  {
    // next uart
    lko->uart.irqno += lko->node;
    lko->uart.base_address += 0x1000UL * lko->node;
    Platform_arm::setup_kernel_options(lko);
  }

  void setup_memory_map() override
  {
    mem_manager->ram->add(
      Region::start_size(Ram_base, Ram_size, ".ram", Region::Ram));
  }
};

}

REGISTER_PLATFORM(Platform_arm_mps3_an536);
