/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "boot_modules.h"

#include <l4/util/elf.h>
#include <l4/sys/compiler.h>

enum : unsigned { PT_CUSTOM_L4_KIP = 0x10, PT_CUSTOM_L4_KOPT = 0x11 };

typedef int exec_handler_func_t(void *opaque, ElfW(Phdr) const *ph,
                                Boot_modules::Module const &m);

int exec_load_elf(exec_handler_func_t *handler_exec, void *opaque,
                  Boot_modules::Module const &m, const char **error_msg);
