/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Authors: Marcus Hähnel <marcus.haehnel@kernkonzept.com>
 */

#include <string.h>
#include <stdio.h>

#include <l4/util/elf.h>
#include <l4/util/mb_info.h>

#include "mm_alloc.h"
#include "support.h"
#include "types.h"

typedef struct
{
  l4util_mb_info_t *mbi;
  l4util_mb_addr_range_t *range;
  unsigned long used; //< Memory used from the current range
} mm_alloc_state;
static mm_alloc_state mm_alloc = { NULL, NULL, 0 };


void
mm_alloc_init(l4util_mb_info_t *mbi)
{
  mm_alloc.mbi = mbi;
  mm_alloc.range = NULL;
}

/**
 * Check if the currently set range actually falls inside the memory map.
 *
 * \retval 0  The range does not belong to the map
 * \retval 1  The range belongs to the map
 *
 * Note: This should be used together with next_ram_range. It is mainly to
 * detect when the iterator has gone beyond the last element. In particular it
 * does not check unaligned access not done by iterating over entries!
 */
static int
mm_alloc_range_in_map(void)
{
  unsigned long addr = (unsigned long)mm_alloc.range;
  l4util_mb_info_t *mbi = mm_alloc.mbi;
  return addr >= mbi->mmap_addr && addr < mbi->mmap_addr + mbi->mmap_length;
}

/**
 * Checks if selected range can accommodate size below 4GiB.
 *
 * Checks if the last address of an allocation of size bytes in the current
 * range would still be within the 4GB that can be catered by the allocator.
 *
 * \param size size of allocation planned from the currently selected range
 *
 * \retval 0  the allocation will not be fully below 4GB
 * \retval 1  the allocation will be completely below 4GB
 */
static int
range_in_4G_for_size(unsigned long size)
{
  return mm_alloc.range->size - mm_alloc.used >= size
         && mm_alloc.range->addr + mm_alloc.used + size <= (1ull << 32);
}

/**
 * Set the allocator to the next ram range in the MBI memory map.
 *
 * This advances the allocator to the next ram range in the memory map that
 * can provide the amount of memory specified in the size parameter.
 *
 * If no range has been set yet (e.g. directly after initialization) then the
 * first range that satisfies the size constraint is selected. If no more
 * ranges can satisfy a request then the current range is set to NULL.
 *
 * If next_ram_range is called again after the last range has been reached
 * iteration will start anew from the first range.
 *
 * Only ranges of type MB_ART_MEMORY are considered. Ranges that start beyond
 * 4GB are also not considered.
 *
 * \param size  The amount of memory that must be at least provided by the range
 */
static void
next_ram_range(unsigned long size)
{
  if (mm_alloc.range == NULL)
    mm_alloc.range = l4util_mb_first_mmap_entry(mm_alloc.mbi);
  else
    mm_alloc.range = l4util_mb_next_mmap_entry(mm_alloc.range);

  mm_alloc.used = 0;

  while (mm_alloc.range != NULL)
    {
      // If the range is suitable just use it
      if (mm_alloc.range->type == MB_ART_MEMORY && range_in_4G_for_size(size))
        return;

      // Otherwise advance to the next
      mm_alloc.range = l4util_mb_next_mmap_entry(mm_alloc.range);
      if (!mm_alloc_range_in_map())
        mm_alloc.range = NULL;
    }
}

void*
mm_alloc_alloc(unsigned long size)
{
  // Not enough memory in current range? Get next range with enough size.
  if (mm_alloc.range == NULL || !range_in_4G_for_size(size))
    next_ram_range(size);

  // Check if what we found overlaps with a reserved region, and retry if yes
  while (mm_alloc.range != NULL)
    {
      void *result = (void*)((unsigned long)mm_alloc.range->addr + mm_alloc.used);

      unsigned long overlap = overlaps_reservation(result, size);
      if (overlap == 0)
        {
          mm_alloc.used += size;
          return result;
        }
      else
        {
          //Advance to the first non-overlapping byte
          mm_alloc.used = overlap - mm_alloc.range->addr;
        }

      // If we now exceeded the free memory in the range use the next one
      if (mm_alloc.range->size <= mm_alloc.used || !range_in_4G_for_size(size))
        next_ram_range(size);
  }

  // Uninitialized or no more available regions
  return NULL;
}
