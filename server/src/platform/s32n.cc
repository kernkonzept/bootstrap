/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#include <l4/drivers/uart_dcc-v6.h>

#include "panic.h"
#include "platform-arm.h"
#include "startup.h"
#include "support.h"

namespace {

class Platform_s32n final : public Platform_arm, public Boot_modules_image_mode
{
  enum : unsigned long
  {
#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_0)
    Rtu_cram_base   = 0x39580000,
    Rtu_cram_end    = 0x39ffffff,
    Rtu_dram_base   = 0x38080000,
    Rtu_tcm_c0_base = 0x26000000,
    Rtu_tcm_c1_base = 0x26400000,
    Rtu_tcm_c2_base = 0x27000000,
    Rtu_tcm_c3_base = 0x27400000,
#elif defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_1)
    Rtu_cram_base   = 0x3b580000,
    Rtu_cram_end    = 0x3bffffff,
    Rtu_dram_base   = 0x3a080000,
    Rtu_tcm_c0_base = 0x28000000,
    Rtu_tcm_c1_base = 0x28400000,
    Rtu_tcm_c2_base = 0x29000000,
    Rtu_tcm_c3_base = 0x29400000,
#elif defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_2)
    Rtu_cram_base   = 0x3d580000,
    Rtu_cram_end    = 0x3dffffff,
    Rtu_dram_base   = 0x3c080000,
    Rtu_tcm_c0_base = 0x2A000000,
    Rtu_tcm_c1_base = 0x2A400000,
    Rtu_tcm_c2_base = 0x2B000000,
    Rtu_tcm_c3_base = 0x2B400000,
#elif defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_3)
    Rtu_cram_base   = 0x3f580000,
    Rtu_cram_end    = 0x3fffffff,
    Rtu_dram_base   = 0x3e080000,
    Rtu_tcm_c0_base = 0x2C000000,
    Rtu_tcm_c1_base = 0x2C400000,
    Rtu_tcm_c2_base = 0x2D000000,
    Rtu_tcm_c3_base = 0x2D400000,
#else
#error "Invalid RTU configuration"
#endif
  };

  /**
   * Only allow dynamic allocations in CRAM area of RTU's.
   */
  static bool validate_area(Region *search_area, [[maybe_unused]] unsigned node)
  {
#ifdef CONFIG_BOOTSTRAP_PF_S32N_DYN_ALLOC_OVERRIDE
    enum : unsigned long {
      Dyn_alloc_begin = CONFIG_BOOTSTRAP_PF_S32N_DYN_ALLOC_START,
      Dyn_alloc_end   = CONFIG_BOOTSTRAP_PF_S32N_DYN_ALLOC_END,
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
#else
    if (search_area->begin() <= Rtu_cram_end
        && search_area->end() >= Rtu_cram_base)
      {
        if (search_area->begin() < Rtu_cram_base)
          search_area->begin(Rtu_cram_base);
        if (search_area->end() > Rtu_cram_end)
          search_area->end(Rtu_cram_end);
        return true;
      }
#endif

    return false;
  }

  bool probe() override
  { return true; }

  unsigned num_nodes() const override
  {
    // Each RTU must run it's own fiasco image
#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_SPLIT) \
    && defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_SPLIT)
    return 4;
#elif (defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_SPLIT) \
    && defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_LOCKSTEP)) \
    || (defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_LOCKSTEP) \
    && defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_SPLIT))
    return 3;
#else
    return 2;
#endif
  }

  unsigned first_node() override
  {
#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_LOCKSTEP) \
    || defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_SPLIT)
    // Cluster0/core0, when cluster0 enabled
    return 0;
#else
    // Cluster1/core1, when cluster0 disabled
    return 2;
#endif
  }

  unsigned current_node() override
  {
    // Each RTU has two clusters with up to 2 cores each -> 0..3
    return node_id();
  }

  static unsigned node_id()
  {
    // Aff0: 0..1 - core in cluster
    // Aff1: 0..1 - cluster in RTU
    // Aff2: 0..3 - RTU in SoC, not used since we run in a single RTU
    unsigned long mpidr;
    asm ("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return (((mpidr & 0x100) >> 7) | (mpidr & 0x1)) & 0x3;
  }

  void init() override
  {
    // Setup core TCMs
    static unsigned long const tcm_bases[4] =
    {
      Rtu_tcm_c0_base, Rtu_tcm_c1_base, Rtu_tcm_c2_base, Rtu_tcm_c3_base
    };
    unsigned long tcm_base = tcm_bases[current_node()];

    // IMP_xTCMREGIONR. ENABLEEL2=1, ENABLEEL10=1
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
    mem_manager->ram->add(Region::start_size(0x24800000U, 256 << 10,
                                             ".AON_RAM", Region::Ram));

#if defined(PLATFORM_TYPE_s32n5)
    mem_manager->ram->add(Region::start_size(0x25c00000U, 3072 << 10,
                                             ".CRS_SRAM", Region::Ram));
#endif

    // RTU CRAM all banks
    mem_manager->ram->add(Region(Rtu_cram_base, Rtu_cram_end,
                                 ".CRAM", Region::Ram));

    // RTU DRAM all banks
    mem_manager->ram->add(Region::start_size(Rtu_dram_base, 1536 << 10,
                                             ".DRAM", Region::Ram));

#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_LOCKSTEP) \
    || defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_SPLIT)
    // RTU/cluster0/core0 TCMs
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c0_base,
                                             64 << 10, ".ATCM00", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c0_base | 0x00100000,
                                             16 << 10, ".BTCM00", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c0_base | 0x00200000,
                                             16 << 10, ".CTCM00", Region::Ram));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_SPLIT)
    // RTU/cluster0/core1 TCMs
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c1_base,
                                             64 << 10, ".ATCM01", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c1_base | 0x00100000,
                                             16 << 10, ".BTCM01", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c1_base | 0x00200000,
                                             16 << 10, ".CTCM01", Region::Ram));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_LOCKSTEP) \
    || defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_SPLIT)
    // RTU/cluster1/core0 TCMs
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c2_base,
                                             64 << 10, ".ATCM10", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c2_base | 0x00100000,
                                             16 << 10, ".BTCM10", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c2_base | 0x00200000,
                                             16 << 10, ".CTCM10", Region::Ram));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL1_SPLIT)
    // RTU/cluster1/core1 TCMs
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c3_base,
                                             64 << 10, ".ATCM11", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c3_base | 0x00100000,
                                             16 << 10, ".BTCM11", Region::Ram));
    mem_manager->ram->add(Region::start_size(Rtu_tcm_c3_base | 0x00200000,
                                             16 << 10, ".CTCM11", Region::Ram));
#endif
  }
};
}

REGISTER_PLATFORM(Platform_s32n);
