/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2015, 2017, 2019-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 */

#pragma once

#include <l4/sys/l4int.h>
#include <l4/util/l4mod.h>
#include <string.h>
#include "region.h"
#include "mod_info.h"

/**
 * Modules created by or internal to bootstrap
 */
struct Internal_module_base
{
  Internal_module_base(const char *cmdline)
  : _next(nullptr), _cmdline(cmdline)
  {}

  unsigned cmdline_size() const { return strlen(_cmdline) + 1; }
  Internal_module_base *next() const { return _next; }
  void next(Internal_module_base *n) { _next = n; }
  virtual void set_region(l4util_l4mod_mod *m) const = 0;

  void set(l4util_l4mod_mod *m, char *cmdline_store) const
  {
    m->cmdline = reinterpret_cast<l4_addr_t>(cmdline_store);
    memcpy(cmdline_store, _cmdline, cmdline_size());
    set_region(m);
    m->flags = 0;
  }

private:
  Internal_module_base *_next;
  const char *_cmdline;
};

struct Internal_module_list
{
  void push_front(Internal_module_base *mod)
  {
    mod->next(root);
    root = mod;
    cnt++;
  }

  Internal_module_base *root = 0;
  unsigned cnt = 0;
};



/**
 * Interface to boot modules.
 *
 * Boot modules can for example be loaded by GRUB, or may be linked
 * into bootstrap.
 */
class Boot_modules
{
public:
  /// Main information for each module.
  struct Module
  {
    char const *start;          ///< The first byte of the module binary.
    char const *end;            ///< The first byte after the module binary.
    char const *cmdline;        ///< Pointer to the module command line.
    Mod_attr_list attrs;        ///< List of module attributes

    unsigned long size() const { return end - start; }
  };

  virtual ~Boot_modules() = 0;
  virtual void reserve() = 0;
  virtual Module module(unsigned index, bool uncompress = true) const = 0;
  virtual unsigned num_modules() const = 0;
  virtual l4util_l4mod_info *construct_mbi(unsigned long mod_addr, Internal_module_list const &mods) = 0;
  virtual void move_module(unsigned index, void *dest) = 0;
  virtual int base_mod_idx(Mod_info_flags mod_info_mod_type,
                           unsigned node = 0) = 0;
  void move_modules(unsigned long modaddr);
  Region mod_region(unsigned index, l4_addr_t start, l4_addr_t size,
                    Region::Type type = Region::Boot);
  void merge_mod_regions();

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
  l4util_l4mod_info *construct_mbi(unsigned long mod_addr, Internal_module_list const &mods) override;
  int base_mod_idx(Mod_info_flags mod_info_module_flag, unsigned node = 0) override;

private:
  void decompress_mods(l4_addr_t total_size, l4_addr_t mod_addr);
};
