/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2020 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */

#include <stdio.h>

#include "support.h"

void L4_NORETURN
panic(char const *str)
{
  printf("PANIC: %s\n", str);
  while (1)
    ;
}

