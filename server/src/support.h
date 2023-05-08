/*!
 * \file   support.h
 * \brief  Support header file
 *
 * \date   2008-01-02
 * \author Adam Lackorznynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __BOOTSTRAP__SUPPORT_H__
#define __BOOTSTRAP__SUPPORT_H__

#include <l4/drivers/uart_base.h>
#include <l4/util/l4mod.h>
#include <l4/util/kip.h>
#include <l4/sys/compiler.h>
#include "mod_info.h"
#include "region.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct boot_args
{
  unsigned long r[4];
};

extern struct boot_args boot_args;

L4::Uart *uart();
void set_stdio_uart(L4::Uart *uart);
void ctor_init();

enum { Verbose_load = 0 };

extern Mod_header *mod_header;
extern Mod_info *module_infos;
void init_modules_infos();

template<typename T>
inline T *l4_round_page(T *p) { return (T*)l4_round_page((l4_addr_t)p); }

template<typename T>
inline T *l4_trunc_page(T *p) { return (T*)l4_trunc_page((l4_addr_t)p); }

static inline void __attribute__((always_inline))
clear_bss()
{
  extern char _bss_start[], _bss_end[];
  extern char crt0_stack_low[], crt0_stack_high[];
  memset(_bss_start, 0, crt0_stack_low - _bss_start);
  memset(crt0_stack_high, 0, _bss_end - crt0_stack_high);
}

static inline unsigned long
round_wordsize(unsigned long s)
{ return (s + sizeof(unsigned long) - 1) & ~(sizeof(unsigned long) - 1); }

struct Memory
{
  Region_list *ram;
  Region_list *regions;
  unsigned long find_free_ram(unsigned long size, unsigned long min_addr = 0,
                              unsigned long max_addr = ~0UL);
  unsigned long find_free_ram_rev(unsigned long size, unsigned long min_addr = 0,
                                  unsigned long max_addr = ~0UL);
};

extern Memory *mem_manager;

static inline
bool
kip_kernel_has_feature(l4_kernel_info_t *kip, const char *feature)
{
  const char *s = l4_kip_version_string(kip);
  if (!s)
    return false;

  l4util_kip_for_each_feature(s)
    if (!strcmp(s, feature))
      return true;

  return false;
}

#endif /* __BOOTSTRAP__SUPPORT_H__ */
