/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/compiler.h>

void
reboot_arch(void) __attribute__((noreturn));

void
reboot_arch(void)
{
  l4_infinite_loop();
}
