/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

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

EXTERN_C void init_cpu_info(void);
