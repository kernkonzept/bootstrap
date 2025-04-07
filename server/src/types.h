/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#include <l4/sys/consts.h>

#define MODS_MAX 128
#define CMDLINE_MAX 1024
#define MOD_NAME_MAX 1024

typedef char __mb_mod_name_str[MOD_NAME_MAX];

// Structure with essential information the boot32 part needs to pass to the
// 64-bit part, for 64bit mode only
struct boot32_info_t
{
  l4_uint32_t rsdp_start;
  l4_uint32_t rsdp_size;
  l4_uint32_t ptab64_addr;
  l4_uint32_t ptab64_size;
  l4_uint64_t mem_end;
};

#endif /* ! __TYPES_H__ */
