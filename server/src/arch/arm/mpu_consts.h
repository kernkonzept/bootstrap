/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

/*
 * Constants that need are needed by assembler code too.
 */

/**
 * Memory Attribute Indirection (MAIR0)
 * Attr0: Device-nGnRnE memory
 * Attr1: Normal memory, Inner/Outer Non-cacheable
 * Attr2: Normal memory, Read-Allocate, no Write-Allocate,
 *        Inner/Outer Write-Through Cacheable (Non-transient)
 *
 * ATTENTION: These must be the same as in Fiasco if the MPU must be left
 * enabled when jumping to Fiasco!
 */

#define MAIR0_BITS 0x00aa4400
#define MAIR1_BITS 0

#define MPU_ATTR_NORMAL     (2 << 1)
#define MPU_ATTR_DEVICE     (0 << 1)
#define MPU_ATTR_BUFFERED   (1 << 1)
