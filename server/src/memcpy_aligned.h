/*
 * Copyright (C) 2020, 2025 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#ifdef __cplusplus
extern "C"
#endif
void memcpy_aligned(void *dst, void const *src, size_t size);
