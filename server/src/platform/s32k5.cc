/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#include <l4/drivers/uart_dcc-v6.h>

#include "panic.h"
#include "platform-arm.h"
#include "startup.h"
#include "support.h"

namespace {

class Platform_s32k5 final : public Platform_arm, public Boot_modules_image_mode
{
  enum : unsigned long
  {
    Main_sram_base  = 0x21000000,
    Main_sram_end   = 0x2117FFFF,
    Cpe_sram_base   = 0x22000000,
    Cpe_sram_end    = 0x220FFFFF,
    Cpe_tcm_c0_base = 0x01000000,
    Cpe_tcm_c1_base = 0x01400000,
    Code_mram_base  = 0x08000000,
    Code_mram_end   = 0x09FFFFFF,
    Data_mram_base  = 0x0C800000,
    Data_mram_end   = 0x0C83FFFF,
  };

  /**
   * Only allow dynamic allocations in SRAM area of CPE.
   */
  static bool validate_area(Region *search_area, [[maybe_unused]] unsigned node)
  {
    enum : unsigned long
    {
#ifdef CONFIG_BOOTSTRAP_PF_S32K5_DYN_ALLOC_OVERRIDE
      Dyn_alloc_begin = CONFIG_BOOTSTRAP_PF_S32K5_DYN_ALLOC_START,
      Dyn_alloc_end   = CONFIG_BOOTSTRAP_PF_S32K5_DYN_ALLOC_END,
#else
      Dyn_alloc_begin = Cpe_sram_base,
      Dyn_alloc_end   = Cpe_sram_end,
#endif
    };

    if (search_area->begin() <= Dyn_alloc_end
        && search_area->end() >= Dyn_alloc_begin)
      {
        if (search_area->begin() < Dyn_alloc_begin)
          search_area->begin(Dyn_alloc_begin);
        if (search_area->end() > Dyn_alloc_end)
          search_area->end(Dyn_alloc_end);
        return true;
      }

    return false;
  }

  bool probe() override
  { return true; }

  unsigned num_nodes() const override
  { return 2; }

  unsigned first_node() override
  { return 0; }

  unsigned current_node() override
  { return node_id(); }

  static unsigned node_id()
  {
    // Aff0: 0..1 - core in cluster
    // Aff1: 0    - cluster in CPE, always 0
    // Aff2: 0    - CPE in SoC, always 0
    unsigned long mpidr;
    asm ("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return mpidr & 0x1;
  }

  void init() override
  {
    // Enable LLPP peripheral port at EL2 and EL1/0
    unsigned long imp_periphpregionr;
    asm volatile ("mrc p15, 0, %0, c15, c0, 0" : "=r"(imp_periphpregionr));
    imp_periphpregionr |= 3;
    asm volatile ("mcr p15, 0, %0, c15, c0, 0" : : "r"(imp_periphpregionr));

    // Enable TCMs at EL2 and EL1/0 (IMP_xTCMREGIONR)
    const unsigned long tcm_base = Cpe_tcm_c0_base;
    asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(tcm_base | 0x00000003));
    asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(tcm_base | 0x00100003));
    asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(tcm_base | 0x00200003));

    // Setup UART
    kuart.access_type = L4_kernel_options::Uart_type_msr;
    static L4::Uart_dcc_v6 _uart;
    set_stdio_uart(&_uart);

    mem_manager->validate = &validate_area;
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    // Main Platform SRAM, all banks
    mem_manager->ram->add(Region(Main_sram_base, Main_sram_end,
                                 ".SRAM", Region::Ram));

    // CPE SRAM, all banks
    mem_manager->ram->add(Region(Cpe_sram_base, Cpe_sram_end,
                                 ".CPE_SRAM", Region::Ram));

    // Code MRAM, all banks
    mem_manager->ram->add(Region(Code_mram_base, Code_mram_end,
                                 ".CMRAM", Region::Ram));

    // Data MRAM, all banks
    mem_manager->ram->add(Region(Data_mram_base, Data_mram_end,
                                 ".DMRAM", Region::Ram));

    // CPE Cortex-R52 TCMs
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c0_base,
                                             32 << 10, ".ATCM0", Region::Ram));
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c0_base | 0x00100000,
                                             32 << 10, ".BTCM0", Region::Ram));
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c0_base | 0x00200000,
                                             32 << 10, ".CTCM0", Region::Ram));
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c1_base,
                                             32 << 10, ".ATCM1", Region::Ram));
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c1_base | 0x00100000,
                                             32 << 10, ".BTCM1", Region::Ram));
    mem_manager->ram->add(Region::start_size(Cpe_tcm_c1_base | 0x00200000,
                                             32 << 10, ".CTCM1", Region::Ram));
  }
};
}

REGISTER_PLATFORM(Platform_s32k5);
