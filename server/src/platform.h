/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015, 2017, 2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include "boot_modules.h"
#include <l4/sys/l4int.h>
#include <stdlib.h> // exit()

class Platform_base
{
public:
  virtual ~Platform_base() = 0;
  virtual void init() = 0;
  virtual void setup_memory_map() = 0;
  virtual Boot_modules *modules() = 0;
  virtual bool probe() = 0;

  virtual l4_uint64_t to_phys(l4_addr_t bootstrap_addr)
  { return bootstrap_addr; }

  virtual l4_addr_t to_virt(l4_uint64_t phys_addr)
  { return phys_addr; }

  virtual void reboot()
  {
    l4_infinite_loop();
  }

  virtual bool have_a_dt() { return false; }

#if defined(ARCH_arm) || defined(ARCH_arm64)
  static void reboot_psci()
  {
    register unsigned long r0 asm("r0") = 0x84000009;
    asm volatile(
#ifdef ARCH_arm
                 ".arch armv7-a\n"
                 ".arch_extension sec\n"
#endif
                 "smc #0" : : "r" (r0));
  }
#endif

  virtual bool arm_switch_to_hyp() { return false; }

  /**
   * Invoked late during startup, when the memory map is already set up and all
   * modules are loaded or moved. This allows allocating memory without the risk
   * of conflicts.
   */
  virtual void late_setup() {};

  virtual void boot_kernel(unsigned long entry)
  {
    typedef void (*func)(void);
    ((func)entry)();
    exit(-100);
  }

  // remember the chosen platform
  static Platform_base *platform;

  // find a platform
  static void iterate_platforms()
  {
    extern Platform_base *__PLATFORMS_BEGIN[];
    extern Platform_base *__PLATFORMS_END[];
    for (Platform_base **p = __PLATFORMS_BEGIN; p < __PLATFORMS_END; ++p)
      if (*p && (*p)->probe())
        {
          platform = *p;
          platform->init();
          break;
        }
  }
};

inline Platform_base::~Platform_base() {}

#define REGISTER_PLATFORM(type) \
      static type type##_inst; \
      static type * const __attribute__((section(".platformdata"),used)) type##_inst_p = &type##_inst

class Platform_single_region_ram : public Platform_base,
  public Boot_modules_image_mode
{
public:
  Boot_modules *modules() override { return this; }
  void setup_memory_map() override;
  virtual void post_memory_hook() {}
};


