/*
 * Copyright (C) 2022, 2024-2025 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/consts.h>

enum
{
  CPUF_PSE        = 1 << 3,
  CPUFEXT_PDPE1GB = 1 << 26,
};

struct cpu_info_t
{
  l4_uint32_t feature_flags;
  l4_uint32_t feature_flags_ext;
};

extern struct cpu_info_t cpu_info;

L4_BEGIN_DECLS

void init_cpu_info(void);

L4_END_DECLS
