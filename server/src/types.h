/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __TYPES_H__
#define __TYPES_H__

#include <l4/sys/consts.h>

#define MODS_MAX 128
#define CMDLINE_MAX 1024
#define MOD_NAME_MAX 1024

typedef char __mb_mod_name_str[MOD_NAME_MAX];

// info where the page table is, for 64bit mode only
struct ptab64_mem_info_t
{
  l4_uint32_t addr;
  l4_uint32_t size;
};

#endif /* ! __TYPES_H__ */
