/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <l4/sys/types.h>

#include "panic.h"
#include "paging.h"
#include "support.h"

EXTERN_C void
ptab_alloc(l4_uint32_t *out_ptab_pa)
{
  // try to find a free region for the page table
  l4_addr_t ptab = mem_manager->find_free_ram_rev(PAGE_SIZE, 0, ~0U);
  if (!ptab)
    panic("fatal: could not allocate memory for page table\n");

  // mark the region as reserved
  mem_manager->regions->add(
    Region::start_size(ptab, PAGE_SIZE, ".ptab", Region::Boot, L4_FPAGE_RW));

  memset((void *)ptab, 0, PAGE_SIZE);
  *out_ptab_pa = ptab;
}
