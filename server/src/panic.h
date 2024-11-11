/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#ifdef __cplusplus
extern "C"
#endif
void panic(const char *fmt, ...) __attribute__((format(printf, 1, 2),noreturn));

