/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "paging.h"
#include "cpu_info.h"

#include <assert.h>

enum
{
  PTAB_LEVEL_PML4E = 3,
  PTAB_LEVEL_PDPE  = 2,
  PTAB_LEVEL_PDE   = 1,
  PTAB_LEVEL_PTE   = 0,

  PTAB_SHIFT_PER_LEVEL = 9,

  PTAB_ENTRY_NUM  = 512,
  PTAB_ENTRY_MASK = PTAB_ENTRY_NUM - 1,
};

static inline l4_uint32_t ptab_shift(l4_uint32_t level)
{ return 12 + (level * PTAB_SHIFT_PER_LEVEL); }

static inline l4_uint64_t ptab_size(l4_uint32_t level)
{ return ((l4_uint64_t)1) << ptab_shift(level); }

static inline l4_uint64_t ptab_mask(l4_uint32_t level)
{ return ptab_size(level) - 1; }

static inline l4_uint64_t* find_ptabe(l4_addr_t ptab_pa, l4_uint32_t level,
                                      l4_uint64_t la)
{
  unsigned entry_idx = (la >> ptab_shift(level)) & PTAB_ENTRY_MASK;
  return &((l4_uint64_t *)ptab_pa)[entry_idx];
}

static inline int is_ptab_aligned(l4_uint32_t level, l4_uint64_t addr)
{ return (addr & ptab_mask(level)) == 0; }

static inline int allow_leaf(l4_uint32_t level)
{
  switch (level)
    {
    case PTAB_LEVEL_PTE: return 1;
    case PTAB_LEVEL_PDE: return cpu_info.feature_flags & CPUF_PSE;
    case PTAB_LEVEL_PDPE: return cpu_info.feature_flags_ext & CPUFEXT_PDPE1GB;
    default /* PTAB_LEVEL_PML4E*/ : return 0;
    }
}

static inline l4_uint32_t leaf_bit(l4_uint32_t level)
{
  if (level == PTAB_LEVEL_PDE || level == PTAB_LEVEL_PDPE)
    return PTAB_PS;
  return 0;
}

static inline int is_leaf(l4_uint32_t level, l4_uint64_t ptabe)
{ return level == PTAB_LEVEL_PTE || (ptabe & leaf_bit(level)); }

static inline void
ptab_map_level(l4_uint32_t level, l4_uint32_t ptab_pa, l4_uint64_t *la,
               l4_uint64_t *pa, l4_uint64_t *size, l4_uint32_t mapping_bits)
{
  do
    {
      l4_uint64_t *ptabe = find_ptabe(ptab_pa, level, *la);
      l4_uint64_t ptab_page_size = ptab_size(level);
      // Map ptab entry as leaf page if possible.
      if (allow_leaf(level))
        {
          // If no mapping exists for the ptab entry, plus the alignment and
          // remaining size allow it, map the ptab entry as a leaf page.
          if (!(*ptabe & PTAB_VALID)
              && is_ptab_aligned(level, *la) && is_ptab_aligned(level, *pa)
              && (*size >= ptab_page_size))
            {
              *ptabe = *pa | PTAB_VALID | mapping_bits | leaf_bit(level);
            }

          // If the ptab entry either was already mapped as a leaf page or we
          // just mapped it in the previous step, adjust the addresses and
          // the remaining size according to the leaf page size.
          if ((*ptabe & PTAB_VALID) && is_leaf(level, *ptabe))
            {
              *la += ptab_page_size;
              *pa += ptab_page_size;
              // Be careful to avoid an underflow of size, in case we
              // encountered an existing leaf page.
              *size -= ptab_page_size <= *size  ? ptab_page_size : *size;
              // We are done with this ptab entry.
              continue;
            }
        }

      // If the ptab entry is not mapped as a leaf page (either not allowed at
      // this level, already mapped as a next level ptab or alignment/size
      // requirements didn't allow for it), descend into next level ptab.
      assert(level > PTAB_LEVEL_PTE);

      // Map ptab entry as next level ptab.
      if (!(*ptabe & PTAB_VALID))
        {
          l4_uint32_t next_ptab_pa;

          // Allocate new page for next level ptab.
          ptab_alloc(&next_ptab_pa);

          // Set the ptab entry to point to it.
          *ptabe = (next_ptab_pa & PTAB_PFN) | PTAB_VALID | mapping_bits;
        }

      // Continue mapping in next level ptab.
      ptab_map_level(level - 1, *ptabe & PTAB_PFN, la, pa, size, mapping_bits);

      // Stop mapping at this level if either all requested memory is mapped or
      // we reached the end of this ptab.
    } while ((*size > 0) && !is_ptab_aligned(level + 1, *la));
}

void
ptab_map_range(l4_uint32_t pml4_pa, l4_uint64_t la, l4_uint64_t pa,
               l4_uint64_t size, l4_uint32_t mapping_bits)
{
  assert(size);
  assert(la+size-1 > la); // avoid wrap around

  // Ensure that linear and physical addresses are page size aligned, and
  // adjust range size accordingly.
  size = round_page(la - trunc_page(la) + size);
  la = trunc_page(la);
  pa = trunc_page(pa);

  ptab_map_level(PTAB_LEVEL_PML4E, pml4_pa, &la, &pa, &size, mapping_bits);
}
