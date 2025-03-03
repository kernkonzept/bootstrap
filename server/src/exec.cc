/**
 * \file	bootstrap/server/src/exec.cc
 * \brief	ELF loader
 *
 * \date	2004
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *		Torsten Frenzel <frenzel@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>

#include <l4/util/elf.h>

#include "exec.h"

int
exec_load_elf(exec_handler_func_t *handler, void *opaque,
              Boot_modules::Module const &m,const char **error_msg)
{
  auto x = reinterpret_cast<ElfW(Ehdr) const *>(m.start);
  /* Read the ELF header.  */

  if (!l4util_elf_check_magic(x))
    return *error_msg="no ELF executable", -1;

  /* Make sure the file is of the right architecture.  */
  if (!l4util_elf_check_arch(x))
    return *error_msg="wrong ELF architecture", -1;

  l4_addr_t phdr = reinterpret_cast<l4_addr_t>(l4util_elf_phdr(x));

  for (int i = 0; i < x->e_phnum; i++)
    {
      auto *ph = reinterpret_cast<ElfW(Phdr) const *>(phdr + i * x->e_phentsize);
      if (ph->p_type == 0)
        continue;

      int res = (*handler)(opaque, ph, m);

      if (res != 0)
        return *error_msg="", res;
    }

  return *error_msg="", 0;
}
