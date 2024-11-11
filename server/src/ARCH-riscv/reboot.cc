/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/compiler.h>
#include "platform.h"

void
reboot_arch(void) __attribute__((noreturn));

void
reboot_arch(void)
{
  Platform_base::platform->reboot();

  l4_infinite_loop();
}
