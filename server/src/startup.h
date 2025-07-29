/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *            Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#ifndef __STARTUP_H__
#define __STARTUP_H__

#include <l4/sys/l4int.h>
#include <l4/util/l4mod.h>
#include <l4/sys/compiler.h>

#include "types.h"

typedef struct
{
  unsigned long kernel_start;
  unsigned long sigma0_start;
  unsigned long roottask_start;
  unsigned long mbi_low, mbi_high;
} boot_info_t;

extern l4_uint64_t mem_end;

//const char * get_cmdline(l4util_mb_info_t *mbi);

#ifdef __cplusplus
#include "koptions-def.h"
char const *check_arg(char const *cmdline, const char *arg);
extern L4_kernel_options::Uart kuart;
extern unsigned int kuart_flags;
extern "C" void startup(char const *cmdline);
#else
extern void startup(char const *cmdline);
#endif

#endif /* ! __STARTUP_H__ */
