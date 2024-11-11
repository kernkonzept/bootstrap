/*
 * (c) 2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#ifndef LOAD_ELF_H__
#define LOAD_ELF_H__ 1

#include "types.h"

void reserve_elf(void const *elf);
l4_uint32_t load_elf(void const *elf);

#endif
