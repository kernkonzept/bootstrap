/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Yann Le Du <ledu@kymasys.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/compiler.h>

#include "platform.h"
#include "support.h"

void
reboot_arch(void) __attribute__((noreturn));

void
reboot_arch(void)
{
  Platform_base::platform->reboot();
  l4_infinite_loop();
}
