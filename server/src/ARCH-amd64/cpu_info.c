/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "cpu_info.h"

struct cpu_info_t cpu_info = { 0, 0 };

static inline void
cpuid(l4_uint32_t mode,
      l4_uint32_t *eax, l4_uint32_t *ebx, l4_uint32_t *ecx, l4_uint32_t *edx)
{
  l4_uint32_t a, b, c, d;
  asm volatile ("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
                        : "a" (mode), "c" (0));
  if (eax) *eax = a;
  if (ebx) *ebx = b;
  if (ecx) *ecx = c;
  if (edx) *edx = d;
}

void
init_cpu_info(void)
{
  l4_uint32_t max_val;
  cpuid(0, &max_val, 0, 0, 0);
  if (max_val >= 1)
    cpuid(1, 0, 0, 0, &cpu_info.feature_flags);

  l4_uint32_t max_ext_val;
  cpuid(0x80000000, &max_ext_val, 0, 0, 0);
  if (max_ext_val >= 0x80000001)
    cpuid(0x80000001, 0, 0, 0, &cpu_info.feature_flags_ext);
}
