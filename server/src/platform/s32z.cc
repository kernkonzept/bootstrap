/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_dcc-v6.h>
#include "panic.h"
#include "support.h"
#include "startup.h"

namespace {

class Platform_s32z : public Platform_base, public Boot_modules_image_mode
{
  enum : unsigned long {
    Rtu0_cram_begin = 0x32100000,
    Rtu0_cram_end   = 0x327fffff,
    Rtu1_cram_begin = 0x36100000,
    Rtu1_cram_end   = 0x367fffff,
  };

  /**
   * Only allow dynamic allocations in CRAM area of RTU0 and RTU1.
   */
  static bool validate_area(Region *search_area)
  {
#ifdef CONFIG_BOOTSTRAP_PF_S32Z_DYN_ALLOC_OVERRIDE
    enum : unsigned long {
      Dyn_alloc_begin = CONFIG_BOOTSTRAP_PF_S32Z_DYN_ALLOC_START,
      Dyn_alloc_end   = CONFIG_BOOTSTRAP_PF_S32Z_DYN_ALLOC_END,
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
    if (search_area->begin() <= Rtu0_cram_end
        && search_area->end() >= Rtu0_cram_begin)
      {
        if (search_area->begin() < Rtu0_cram_begin)
          search_area->begin(Rtu0_cram_begin);
        if (search_area->end() > Rtu0_cram_end)
          search_area->end(Rtu0_cram_end);
        return true;
      }

    if (search_area->begin() <= Rtu1_cram_end
        && search_area->end() >= Rtu1_cram_begin)
      {
        if (search_area->begin() < Rtu1_cram_begin)
          search_area->begin(Rtu1_cram_begin);
        if (search_area->end() > Rtu1_cram_end)
          search_area->end(Rtu1_cram_end);
        return true;
      }
#endif

    return false;
  }

  bool probe() override { return true; }

  unsigned num_nodes() override
  { return 8; }

  unsigned current_node() override final
  {
    unsigned long mpidr;
    asm ("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return ((mpidr >> 6) | mpidr) & 0x07;
  }

  void init() override
  {
    // Enable peripheral access through LLPP
    unsigned long imp_periphpregionr;
    asm volatile ("mrc p15, 0, %0, c15, c0, 0" : "=r"(imp_periphpregionr));
    imp_periphpregionr |= 3; // enable port @ EL2 and EL1/0
    asm volatile ("mcr p15, 0, %0, c15, c0, 0" : : "r"(imp_periphpregionr));

    // Split cache ways between AXIF and AXIM
    asm volatile ("mcr p15, 1, %0, c9, c1, 0" : : "r"(0x202));

    kuart.access_type = L4_kernel_options::Uart_type_msr;
    static L4::Uart_dcc_v6 _uart;
    set_stdio_uart(&_uart);

    // Enable TCMs
    if (current_node() == 0)
      {
        asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(0x30000003));
        asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(0x30100003));
        asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(0x30200003));
      }
    else if (current_node() == 4)
      {
        asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(0x34000003));
        asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(0x34100003));
        asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(0x34200003));
      }
    else
      panic("Running on invalid core!\n");

    mem_manager->validate = &validate_area;
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    mem_manager->ram->add(Region(0x25000000U, 0x250fffffU,
                                 ".SMU", Region::Ram, 0, false, 0xff));

#if defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU0_LOCKSTEP) \
    || defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU0_SPLIT)
    // RTU0 [CD]RAM
    mem_manager->ram->add(Region(Rtu0_cram_begin, Rtu0_cram_end,
                                 ".CRAM0", Region::Ram, 0, false, 0x0f));
    mem_manager->ram->add(Region(0x31780000U, 0x3187ffffU,
                                 ".DRAM0", Region::Ram, 0, false, 0x0f));

    // RTU0 core 0 TCMs
    mem_manager->ram->add(Region(0x30000000U, 0x3000ffffU,
                                 ".ATCM00", Region::Ram, 0, false, 0x01));
    mem_manager->ram->add(Region(0x30100000U, 0x30103fffU,
                                 ".BTCM00", Region::Ram, 0, false, 0x01));
    mem_manager->ram->add(Region(0x30200000U, 0x30203fffU,
                                 ".CTCM00", Region::Ram, 0, false, 0x01));

    // RTU0 core 1 TCMs
    mem_manager->ram->add(Region(0x30400000U, 0x3040ffffU,
                                 ".ATCM01", Region::Ram, 0, false, 0x02));
    mem_manager->ram->add(Region(0x30500000U, 0x30503fffU,
                                 ".BTCM01", Region::Ram, 0, false, 0x02));
    mem_manager->ram->add(Region(0x30600000U, 0x30603fffU,
                                 ".CTCM01", Region::Ram, 0, false, 0x02));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU0_SPLIT)
    // Add core 2+3 TCMs if RTU0 is running in split-lock config
    mem_manager->ram->add(Region(0x30800000U, 0x3080ffffU,
                                 ".ATCM02", Region::Ram, 0, false, 0x04));
    mem_manager->ram->add(Region(0x30900000U, 0x30903fffU,
                                 ".BTCM02", Region::Ram, 0, false, 0x04));
    mem_manager->ram->add(Region(0x30a00000U, 0x30a03fffU,
                                 ".CTCM02", Region::Ram, 0, false, 0x04));

    mem_manager->ram->add(Region(0x30c00000U, 0x30c0ffffU,
                                 ".ATCM03", Region::Ram, 0, false, 0x08));
    mem_manager->ram->add(Region(0x30d00000U, 0x30d03fffU,
                                 ".BTCM03", Region::Ram, 0, false, 0x08));
    mem_manager->ram->add(Region(0x30e00000U, 0x30e03fffU,
                                 ".CTCM03", Region::Ram, 0, false, 0x08));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU1_LOCKSTEP) \
    || defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU1_SPLIT)
    // RTU1 [CD]RAM
    mem_manager->ram->add(Region(Rtu1_cram_begin, Rtu1_cram_end,
                                 ".CRAM1", Region::Ram, 0, false, 0xf0));
    mem_manager->ram->add(Region(0x35780000U, 0x3587ffffU,
                                 ".DRAM1", Region::Ram, 0, false, 0xf0));

    // RTU1 core 0 TCMs
    mem_manager->ram->add(Region(0x34000000U, 0x3400ffffU,
                                 ".ATCM10", Region::Ram, 0, false, 0x10));
    mem_manager->ram->add(Region(0x34100000U, 0x34103fffU,
                                 ".BTCM10", Region::Ram, 0, false, 0x10));
    mem_manager->ram->add(Region(0x34200000U, 0x34203fffU,
                                 ".CTCM10", Region::Ram, 0, false, 0x10));

    // RTU1 core 1 TCMs
    mem_manager->ram->add(Region(0x34400000U, 0x3440ffffU,
                                 ".ATCM11", Region::Ram, 0, false, 0x20));
    mem_manager->ram->add(Region(0x34500000U, 0x34503fffU,
                                 ".BTCM11", Region::Ram, 0, false, 0x20));
    mem_manager->ram->add(Region(0x34600000U, 0x34603fffU,
                                 ".CTCM11", Region::Ram, 0, false, 0x20));
#endif

#if defined(CONFIG_BOOTSTRAP_PF_S32Z_RTU1_SPLIT)
    // Add core 2+3 TCMs if RTU1 is running in split-lock config
    mem_manager->ram->add(Region(0x34800000U, 0x3480ffffU,
                                 ".ATCM12", Region::Ram, 0, false, 0x40));
    mem_manager->ram->add(Region(0x34900000U, 0x34903fffU,
                                 ".BTCM12", Region::Ram, 0, false, 0x40));
    mem_manager->ram->add(Region(0x34a00000U, 0x34a03fffU,
                                 ".CTCM12", Region::Ram, 0, false, 0x40));

    mem_manager->ram->add(Region(0x34c00000U, 0x34c0ffffU,
                                 ".ATCM13", Region::Ram, 0, false, 0x80));
    mem_manager->ram->add(Region(0x34d00000U, 0x34d03fffU,
                                 ".BTCM13", Region::Ram, 0, false, 0x80));
    mem_manager->ram->add(Region(0x34e00000U, 0x34e03fffU,
                                 ".CTCM13", Region::Ram, 0, false, 0x80));
#endif
  }

  void reboot() override
  {
    for (;;);
  }
};
}

REGISTER_PLATFORM(Platform_s32z);
