/*
 * Copyright (C) 2008-2026 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   memory.h
 * \brief  Memory manager.
 */

#pragma once

#include <l4/sys/consts.h>

class Region;
class Region_list;

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
