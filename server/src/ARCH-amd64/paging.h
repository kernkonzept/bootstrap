/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

#include <l4/sys/consts.h>

enum
{
  PAGE_SIZE      = (1 << 12),
  PAGE_MASK      = (PAGE_SIZE - 1),
  // Physical Address Extension is enabled, thus superpages have a size of 2MB.
  SUPERPAGE_SIZE = (1 << 21),
  SUPERPAGE_MASK = (SUPERPAGE_SIZE - 1),

  PTAB_VALID = 0x0000000000000001LL,
  PTAB_WRITE = 0x0000000000000002LL,
  PTAB_USER  = 0x0000000000000004LL,
  PTAB_PFN   = 0x000ffffffffff000LL,
  PTAB_PS    = 0x0000000000000080LL,
};

static inline l4_uint64_t trunc_page(l4_uint64_t x)
{ return x & ~PAGE_MASK; }

static inline l4_uint64_t round_page(l4_uint64_t x)
{ return trunc_page(x + PAGE_MASK); }

static inline l4_uint64_t round_superpage(l4_uint64_t x)
{ return (x + SUPERPAGE_MASK) & ~SUPERPAGE_MASK; }

EXTERN_C_BEGIN

/**
 * Allocate and zero-initialize a page table.
 *
 * \param[out] out_ptab_pa  Physical address of the allocated page table.
 *
 * \note ptab_map_range() uses this function to allocate intermediate page
 *       tables. Thus, users of ptab_map_range() must provide an implementation
 *       of this function. ptab_map_range() expects that the physical memory
 *       from which the page tables are allocated, is identity mapped in the
 *       linear address space.
 */
void ptab_alloc(l4_uint32_t *out_ptab_pa);

/**
 * Maps a physical memory range into the linear address space spanned/organized
 * in a 4-level page table hierarchy.
 *
 * Existing mappings are left intact, i.e. this function only maps the parts of
 * the given range that are not yet mapped. The underlying assumption is that we
 * never try to map a a linear address to different physical addresses or with
 * different mappings bits than it was initially mapped with.
 * This is useful because as we are only interested in identity mappings, we can
 * treat mapping physical memory as an idempotent operation, allowing us to map
 * physical memory on demand, without checking if it is already mapped.
 *
 * \param[in] pml4_pa       The root level page table.
 * \param[in] la            Linear base address.
 * \param[in] pa            Physical base address.
 * \param[in] size          The size of the range to be mapped.
 * \param[in] mapping_bits  The bits the mapping should be done with.
 */
void ptab_map_range(l4_uint32_t pml4_pa, l4_uint64_t la, l4_uint64_t pa,
                    l4_uint64_t size, l4_uint32_t mapping_bits);

EXTERN_C_END
