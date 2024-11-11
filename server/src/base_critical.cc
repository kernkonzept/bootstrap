/*
 * (c) 2008-2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/util/irq.h>
#include "base_critical.h"

static l4_umword_t flags;
static l4_umword_t entry_count;

void
base_critical_enter(void)
{
  if (entry_count == 0)
    {
      l4util_flags_save(&flags);
      l4util_cli();
    }
  entry_count++;
}

void
base_critical_leave(void)
{
  entry_count--;
  if (entry_count == 0)
    l4util_flags_restore(&flags);
}

