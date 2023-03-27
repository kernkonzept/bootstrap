/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@l4re.org>
 */

#include "boot_modules.h"
#include "dt.h"
#include "platform.h"

template<typename BASE>
class Platform_dt : public BASE,
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

  virtual l4_addr_t get_fdt_addr() const = 0;

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    dt.check_for_dt();
    dt.setup_memory();
  }

  void init_dt(Internal_module_list &mods) override
  {
    dt.init(get_fdt_addr());
    if (dt.have_fdt())
      mods.push_front(&mod_fdt);
  }

  Dt dt;
};
