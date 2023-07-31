/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@os.inf.tu-dresden.de>
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
