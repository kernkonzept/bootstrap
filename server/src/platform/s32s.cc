/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_linflex.h>
#include "support.h"
#include "startup.h"
#include "arch/arm/mem.h"
#include "arch/arm/mpu.h"

namespace {

using Arm::Mpu;

class Platform_s32s : public Platform_base, public Boot_modules_image_mode
{
  bool probe() { return true; }

  unsigned num_nodes() override
  { return 4; }

  void init()
  {
    Mpu::init();
    Mpu::set(0, Mpu::Region(0x00000000, 0x00ffffff, Mpu::Region::Prot::RO,
                            Mpu::Region::Type::Normal));
    Mpu::set(1, Mpu::Region(0x34000000, 0x34ffffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(2, Mpu::Region(0x40000000, 0xffffffff, 0,
                            Mpu::Region::Type::Device));
    Mpu::enable();

    Cache::Insn::enable();
    Cache::Data::inv();
    Cache::Data::enable();

    // Write timer frequency to CNTFRQ. The kernel expects a valid value.
    asm volatile ("MCR p15, 0, %0, c14, c0, 0" : : "r"(20000000));

    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.base_address = 0x401c8000;
    kuart.base_baud    = 48000000;
    kuart.irqno        = 32 + 82;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_linflex _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void boot_kernel(unsigned long entry) override
  {
    Cache::Data::flush();
    Cache::Data::disable();
    Cache::Insn::disable();
    Mpu::disable();
    Platform_base::boot_kernel(entry);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    mem_manager->ram->add(Region(0x34000000U, 0x35000000U,
                                 ".ram", Region::Ram));
  }

  void reboot()
  {
    for (;;);
  }
};
}

REGISTER_PLATFORM(Platform_s32s);
