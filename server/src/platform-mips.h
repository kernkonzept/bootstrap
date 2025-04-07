/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "mips-defs.h"
#include "platform.h"

class Platform_mips : public Platform_base
{
public:
  bool probe() override
  { return true; }

  l4_uint64_t to_phys(l4_addr_t bootstrap_addr) override
  { return bootstrap_addr - Mips::KSEG0; }

  l4_addr_t to_virt(l4_uint64_t phys_addr) override
  { return phys_addr + Mips::KSEG0; }
};
