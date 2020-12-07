/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Authors: Marcus Hähnel <marcus.haehnel@kernkonzept.com>
 */

#pragma once

#include <l4/util/mb_info.h>

/**
 * Function to initialize the global state of the memory allocator.
 *
 * \param mbi  Pointer to the multiboot information structure provided by the
 *             bootloader
 */
void mm_alloc_init(l4util_mb_info_t *mbi) __attribute__((weak));


/**
 * Allocate memory from the mm_alloc allocator
 *
 * Returns a pointer to a chunk of memory of the specified size. It is
 * guaranteed that the returned memory does not overlap with
 *
 *   * the bootstrap image itself
 *   * any modules passed through multiboot
 *   * the multiboot information, except information pertaining to sections
 *     of this bootstrap ELF (not the inner ELF!).
 *
 * \param size   The size of the memory that is to be allocated
 * \returns      A pointer to the newly allocated memory
 * \retval NULL  if not enough memory was available or the allocator was not
 *               initialized.
 */
void *mm_alloc_alloc(unsigned long size) __attribute__((weak));
