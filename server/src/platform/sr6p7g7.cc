/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/drivers/uart_dcc-v6.h>

#include "arch/arm/mem.h"
#include "arch/arm/mpu.h"
#include "platform-arm.h"
#include "startup.h"

namespace {

using Arm::Mpu;

class Platform_sr6p7g7 : public Platform_arm, public Boot_modules_image_mode
{
  bool probe() { return true; }

  unsigned num_nodes() override
  { return 6; }

  // Three clusters (Aff1) with up to 2 cores each (Aff0) -> 0..5
  unsigned current_node() override
  {
    unsigned long mpidr;
    asm ("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return ((mpidr >> 7) | mpidr) & 0x07;
  }

  // Constrain node-local allocations to cluster RAM. Otherwise use system RAM.
  static bool validate_area(Region *search_area, unsigned node)
  {
    unsigned cluster = node < 6 ? (node >> 1) : ~0U;

    static constexpr struct
    {
      unsigned long begin;
      unsigned long end;
      unsigned cluster;
    } const cluster_regions[] = {
      { 0x60000000UL, 0x6007ffff,  0U }, // Cluster 0
      { 0x60400000UL, 0x6047ffff,  0U }, // Cluster 0
      { 0x60800000UL, 0x6085ffff,  1U }, // Cluster 1
      { 0x60c00000UL, 0x60c5ffff,  1U }, // Cluster 1
      { 0x61000000UL, 0x6103ffff,  2U }, // Cluster 2
      { 0x61400000UL, 0x6143ffff,  2U }, // Cluster 2
      { 0x64000000UL, 0x6403ffff, ~0U }, // SYSRAM
      { 0x64400000UL, 0x6443ffff, ~0U }, // SYSRAM
    };

    for (auto const &r : cluster_regions)
      {
        if (r.cluster == cluster
            && search_area->begin() <= r.begin
            && search_area->end() >= r.end)
          {
            if (search_area->begin() < r.begin)
              search_area->begin(r.begin);
            if (search_area->end() > r.end)
              search_area->end(r.end);
            return true;
          }
      }

    return false;
  }

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

    mem_manager->validate = &validate_area;
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
    mem_manager->ram->add(Region::start_size(0x60000000U, 512 << 10,
                                             ".RAM00", Region::Ram));
    mem_manager->ram->add(Region::start_size(0x60400000U, 512 << 10,
                                             ".RAM01", Region::Ram));

    mem_manager->ram->add(Region::start_size(0x60800000U, 384 << 10,
                                             ".RAM10", Region::Ram));
    mem_manager->ram->add(Region::start_size(0x60c00000U, 384 << 10,
                                             ".RAM11", Region::Ram));

    mem_manager->ram->add(Region::start_size(0x61000000U, 256 << 10,
                                             ".RAM20", Region::Ram));
    mem_manager->ram->add(Region::start_size(0x61400000U, 256 << 10,
                                             ".RAM21", Region::Ram));

    mem_manager->ram->add(Region::start_size(0x64000000U, 256 << 10,
                                             ".SYSRAM0", Region::Ram));
    mem_manager->ram->add(Region::start_size(0x64400000U, 256 << 10,
                                             ".SYSRAM1", Region::Ram));
  }
};

} /* namespace */

REGISTER_PLATFORM(Platform_sr6p7g7);
