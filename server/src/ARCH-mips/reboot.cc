/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
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
