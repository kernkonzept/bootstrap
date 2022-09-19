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

void init_modules_infos();

template<typename T>
inline T *l4_round_page(T *p)
{
  return reinterpret_cast<T*>(l4_round_page(reinterpret_cast<l4_addr_t>(p)));
}

template<typename T>
inline T *l4_trunc_page(T *p)
{
  return reinterpret_cast<T*>(l4_trunc_page(reinterpret_cast<l4_addr_t>(p)));
}

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
                              unsigned long max_addr = ~0UL,
                              unsigned align = L4_PAGESHIFT,
                              unsigned node = ~0U);
  unsigned long find_free_ram_rev(unsigned long size, unsigned long min_addr = 0,
                                  unsigned long max_addr = ~0UL,
                                  unsigned align = L4_PAGESHIFT,
                                  unsigned node = ~0U);

  /**
   * Optional callback to constrain dynamic allocations.
   *
   * The function may narrow the `search_area`.
   *
   * @param search_area   A candidate RAM area.
   * @param node          Applicable AMP node or ~0U if unspecified.
   *
   * @returns   True if area can be used, otherwise false.
   */
  bool (*validate)(Region *search_area, unsigned node);
};

extern Memory *mem_manager;

#endif /* __BOOTSTRAP__SUPPORT_H__ */
