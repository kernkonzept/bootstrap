/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_dcc-v6.h>
#include "support.h"
#include "startup.h"
#include "arch/arm/mem.h"
#include "arch/arm/mpu.h"
#include "panic.h"

namespace {

using Arm::Mpu;

class Platform_s32s : public Platform_base, public Boot_modules_image_mode
{
  bool probe() { return true; }

  unsigned num_nodes() override
  { return 6; }

  void init()
  {
    Mpu::disable(); // The ROM code enables the MPU already. Caches are disabled.

    Mpu::init();
    Mpu::set(0, Mpu::Region(0x60000000, 0x6007ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(1, Mpu::Region(0x70000000, 0x7fffffff, Mpu::Region::Prot::NX,
                            Mpu::Region::Type::Device));
    Mpu::set(2, Mpu::Region(0x60400000, 0x6047ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(3, Mpu::Region(0x64000000, 0x6403ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(4, Mpu::Region(0x64400000, 0x6443ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(5, Mpu::Region(0x50000000, 0x500fffff, Mpu::Region::Prot::RO,
                            Mpu::Region::Type::Normal));

    Mpu::set(6, Mpu::Region(0x60800000, 0x6085ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(7, Mpu::Region(0x60c00000, 0x60c5ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(8, Mpu::Region(0x61000000, 0x6103ffff, 0,
                            Mpu::Region::Type::Normal));
    Mpu::set(9, Mpu::Region(0x61400000, 0x6143ffff, 0,
                            Mpu::Region::Type::Normal));

    Mpu::enable();

    Cache::Insn::enable();
    Cache::Data::inv();
    Cache::Data::enable();

    // Write timer frequency to CNTFRQ. Required for non-hyp kernels.
    asm volatile ("MCR p15, 0, %0, c14, c0, 0" : : "r"(1000000000));

    // XXX: disable EL2 checks in PBRIDGE
    *((l4_uint32_t volatile *)0x70000368) = 0x01;
    *((l4_uint32_t volatile *)0x70600368) = 0x01;
    *((l4_uint32_t volatile *)0x70c00368) = 0x01;
    *((l4_uint32_t volatile *)0x71200368) = 0x01;
    *((l4_uint32_t volatile *)0x71800368) = 0x01;

    kuart.access_type  = L4_kernel_options::Uart_type_msr;
    kuart.irqno = -1;
    static L4::Uart_dcc_v6 _uart;
    set_stdio_uart(&_uart);
  }

  void boot_kernel(unsigned long entry) override
  {
    Cache::Data::flush();
    Cache::Data::disable();
    Cache::Insn::disable();

    Platform_base::boot_kernel(entry);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    //mem_manager->ram->add(Region::start_size(0x50000000U, 1024 << 10,
    //                                         ".OVL0", Region::Ram, 0, false,
    //                                         0x03));

    mem_manager->ram->add(Region::start_size(0x60000000U, 512 << 10,
                                             ".RAM00", Region::Ram, 0, false,
                                             0x03));
    mem_manager->ram->add(Region::start_size(0x60400000U, 512 << 10,
                                             ".RAM01", Region::Ram, 0, false,
                                             0x03));

    mem_manager->ram->add(Region::start_size(0x60800000U, 384 << 10,
                                             ".RAM10", Region::Ram, 0, false,
                                             0x0c));
    mem_manager->ram->add(Region::start_size(0x60c00000U, 384 << 10,
                                             ".RAM11", Region::Ram, 0, false,
                                             0x0c));

    mem_manager->ram->add(Region::start_size(0x61000000U, 256 << 10,
                                             ".RAM20", Region::Ram, 0, false,
                                             0x30));
    mem_manager->ram->add(Region::start_size(0x61400000U, 256 << 10,
                                             ".RAM21", Region::Ram, 0, false,
                                             0x30));

    mem_manager->ram->add(Region::start_size(0x64000000U, 256 << 10,
                                             ".SYSRAM0", Region::Ram, 0, false,
                                             0x3f));
    mem_manager->ram->add(Region::start_size(0x64400000U, 256 << 10,
                                             ".SYSRAM1", Region::Ram, 0, false,
                                             0x3f));
  }

  void reboot()
  {
    for (;;);
  }
};
}

REGISTER_PLATFORM(Platform_s32s);

