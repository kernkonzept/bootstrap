/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@l4re.org>
 */

#include <l4/util/l4mod.h>
#include "platform.h"
#include "boot_modules.h"
#include "dt.h"

class Platform_dt : public Platform_base,
                    public Boot_modules_image_mode
{
  struct Dt_module : Internal_module_base
  {
    Dt_module(const char *cmdline, Dt const &dt)
    : Internal_module_base(cmdline), dt(dt)
    {}

    void set_region(l4util_l4mod_mod *m) const override
    {
      m->mod_start = (unsigned long)dt.fdt();
      m->mod_end   = (unsigned long)dt.fdt() + dt.fdt_size();
    }

    Dt const &dt;
  };

  Dt_module mod_fdt;
public:
  Platform_dt() : mod_fdt(".fdt", dt) {}
  Boot_modules *modules() override { return this; }
  void setup_memory_map() override;
  virtual void post_memory_hook() {}
  void init_dt(unsigned long fdt_addr, Internal_module_list &mods) override
  {
    dt.init(fdt_addr);
    if (dt.have_fdt())
      mods.push_front(&mod_fdt);
  }

  void setup_spin_addr(L4_kernel_options::Options *lko) override
  {
    lko->core_spin_addr = dt.have_fdt() ? dt.cpu_release_addr() : -1ULL;
    if (lko->core_spin_addr == -1ULL)
      Platform_base::setup_spin_addr(lko);
  }

  Dt dt;
};
