/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015, 2017, 2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include <l4/sys/l4int.h>
#include <l4/util/l4mod.h>
#include "region.h"
#include "mod_info.h"

/**
 * Interface to boot modules.
 *
 * Boot modules can for example be loaded by GRUB, or may be linked
 * into bootstrap.
 */
class Boot_modules
{
public:
  enum { Num_base_modules = 3 };

  /// Main information for each module.
  struct Module
  {
    char const *start;
    char const *end;
    char const *cmdline;

    unsigned long size() const { return end -start; }
  };

  static char const *const Mod_reg;

  virtual ~Boot_modules() = 0;
  virtual void reserve() = 0;
  virtual Module module(unsigned index, bool uncompress = true) const = 0;
  virtual unsigned num_modules() const = 0;
  virtual l4util_l4mod_info *construct_mbi(unsigned long mod_addr) = 0;
  virtual void move_module(unsigned index, void *dest) = 0;
  virtual int base_mod_idx(Mod_info_flags mod_info_mod_type) = 0;
  void move_modules(unsigned long modaddr);
  Region mod_region(unsigned index, l4_addr_t start, l4_addr_t size,
                    Region::Type type = Region::Boot);
  void merge_mod_regions();
  static bool is_base_module(const Mod_info *mod)
  {
    unsigned v = mod->flags & Mod_info_flag_mod_mask;
    return v > 0 && v <= Num_base_modules;
  };

protected:
  void _move_module(unsigned index, void *dest, void const *src,
                    unsigned long size);
};

inline Boot_modules::~Boot_modules() {}

/**
 * For image mode we have this utility that implements
 * handling of linked in modules.
 */
class Boot_modules_image_mode : public Boot_modules
{
public:
  void reserve() override;
  Module module(unsigned index, bool uncompress) const override;
  unsigned num_modules() const override;
  void move_module(unsigned index, void *dest) override;
  l4util_l4mod_info *construct_mbi(unsigned long mod_addr) override;
  int base_mod_idx(Mod_info_flags mod_info_module_flag) override;

private:
  void decompress_mods(unsigned mod_count,
                       l4_addr_t total_size, l4_addr_t mod_addr);
};

