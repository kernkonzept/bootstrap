/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2008-2009 Technische Universität Dresden
 * Copyright (C) 2023 Kernkonzept GmbH
 * Authors:
 *               Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */
#pragma once

#include <l4/sys/kip.h>

#include "startup.h"
#include "region.h"

void init_kip(l4_kernel_info_t *l4i, boot_info_t *bi, l4util_l4mod_info *mbi,
              Region_list *ram, Region_list *regions);

#if defined(ARCH_ppc32)
void init_kip_v2_arch(l4_kernel_info_t *);
#endif
