/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015, 2017, 2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include "boot_modules.h"
#include "koptions-def.h"
#include <l4/sys/l4int.h>
#include <l4/sys/kip.h>
#include <stdlib.h> // exit()

class Platform_base
{
public:
  virtual ~Platform_base() = 0;
  virtual void init() = 0;
  virtual void setup_memory_map() = 0;
  virtual void exit_boot_services() { }
  virtual Boot_modules *modules() = 0;
  virtual bool probe() = 0;

  virtual l4_uint64_t to_phys(l4_addr_t bootstrap_addr)
  { return bootstrap_addr; }

  virtual l4_addr_t to_virt(l4_uint64_t phys_addr)
  { return phys_addr; }

  /** Number of AMP nodes on platform. */
  virtual unsigned num_nodes()
  { return 1; }

  /** Id of first AMP node on platform. */
  virtual unsigned first_node()
  { return 0; }

  /** Id of current AMP on platform. */
  virtual unsigned current_node()
  { return 0; }

  virtual void reboot()
  {
    l4_infinite_loop();
  }

  virtual void init_dt(Internal_module_list &) {}

  /**
   * Invoked late during startup, when the memory map is already set up and all
   * modules are loaded or moved. This allows allocating memory without the risk
   * of conflicts.
   */
  virtual void late_setup(l4_kernel_info_t *) {};

  /**
   * Called immediately after loading the kernel. Beware that the KIP has not
   * yet been initialized at that point. In particular you should not assume
   * that the memory configuration, the platform name or the pointers to root
   * task / pager are valid.
   */
  virtual void setup_kernel_config(l4_kernel_info_t*) {};

  /**
   * Called after all other kernel options have been set up such that the
   * platform can set platform-specific kernel options.
   */
  virtual void setup_kernel_options(L4_kernel_options::Options *) {}

  virtual void init_regions() {}

  virtual void boot_kernel(unsigned long entry)
  {
    typedef void (*func)(void);
    reinterpret_cast<func>(entry)();
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


void setup_single_region_ram_memory_map();

template<class BASE>
class Platform_single_region_ram : public BASE,
  public Boot_modules_image_mode
{
public:
  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    setup_single_region_ram_memory_map();
    post_memory_hook();
  }

  virtual void post_memory_hook() {}
};


