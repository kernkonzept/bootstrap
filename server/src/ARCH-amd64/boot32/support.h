/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2020 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */

#pragma once

#include <l4/sys/compiler.h>

/**
 * Output a string and go into endless loop
 *
 * \param str The string to print
 */
void L4_NORETURN panic(char const *str);
