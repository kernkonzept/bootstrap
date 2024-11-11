/*
 * (c) 2008-2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/types.h>

/* We need this typecasts since addresses of the GRUB multiboot info
 * have always a size of 32 Bit. */

#define L4_CONST_CHAR_PTR(x)	reinterpret_cast<const char*>(\
                                  static_cast<l4_addr_t>(x))
