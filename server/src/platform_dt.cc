/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@l4re.org>
 */

#include "platform_dt.h"
#include "dt.h"

void
Platform_dt::setup_memory_map()
{
  dt.check_for_dt();
  dt.setup_memory();
}
