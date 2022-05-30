/**
 * \file
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>
#include <l4/sys/kip.h>
#include <l4/util/l4_macros.h>
#include "panic.h"
#include "assert.h"

#include "macros.h"
#include "init_kip.h"
#include "region.h"
#include "startup.h"
#include <l4/sys/kip>

using L4::Kip::Mem_desc;

/**
 * setup Kernel Info Page
 */
void
init_kip(l4_kernel_info_t *l4i, boot_info_t *bi, l4util_l4mod_info *mbi,
         Region_list *ram, Region_list *regions)
{
  if (L4_KIP_VERSION_FIASCO != l4_kip_version(l4i))
    panic("cannot load kernels other than Fiasco");

  Mem_desc *md = Mem_desc::first(l4i);
  assert((unsigned long)md - (unsigned long)l4i >= sizeof(*l4i));

  for (Region const* c = ram->begin(); c != ram->end(); ++c)
    {
      // Exclude any non 1K-aligned conventional memory.
      unsigned long long begin = l4_round_size(c->begin(), 10);
      unsigned long long end = l4_trunc_size(c->end() + 1, 10) - 1;
      (md++)->set(begin, end, Mem_desc::Conventional);
    }

  for (Region const *c = regions->begin(); c != regions->end(); ++c)
    {
      Mem_desc::Mem_type type = Mem_desc::Reserved;
      unsigned char sub_type = 0;
      switch (c->type())
        {
        case Region::No_mem:
        case Region::Ram:
        case Region::Boot:
          continue;
        case Region::Kernel:
          type = Mem_desc::Reserved;
          break;
        case Region::Sigma0:
          type = Mem_desc::Dedicated;
          break;
        case Region::Root:
          type = Mem_desc::Bootloader;
          sub_type = c->sub_type();
          break;
        case Region::Arch:
          type = Mem_desc::Arch;
          sub_type = c->sub_type();
          break;
        case Region::Info:
          type = Mem_desc::Info;
          sub_type = c->sub_type();
          break;
        }
      (md++)->set(c->begin(), c->end() - 1, type, sub_type, false,
                  c->eager());
    }

  l4i->user_ptr = reinterpret_cast<unsigned long>(mbi);

  /* set up sigma0 info */
  l4i->sigma0_eip = bi->sigma0_start;
  printf("  Sigma0 config    node: %d   ip:" l4_addr_fmt "\n",
         l4i->node, l4i->sigma0_eip);

  /* set up roottask info */
  l4i->root_eip = bi->roottask_start;
  printf("  Roottask config  node: %d   ip:" l4_addr_fmt "\n",
         l4i->node, l4i->root_eip);

  /* Platform info */
  strncpy(l4i->platform_info.name, PLATFORM_TYPE,
          sizeof(l4i->platform_info.name));
  l4i->platform_info.name[sizeof(l4i->platform_info.name) - 1] = 0;
}
