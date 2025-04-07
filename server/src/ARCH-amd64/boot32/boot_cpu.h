/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#ifndef BOOT_CPU_H
#define BOOT_CPU_H

#include "types.h"

void base_paging_init (l4_uint64_t);
void base_cpu_setup (void);

extern struct boot32_info_t boot32_info;

#endif
