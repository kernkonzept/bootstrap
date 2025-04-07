/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#ifndef __BOOTSTRAP__UNCOMPRESS_H__
#define __BOOTSTRAP__UNCOMPRESS_H__

void *decompress(const char *name, const char *start, char *destbuf,
                 int size, int size_uncompressed);

#endif /* ! __BOOTSTRAP__UNCOMPRESS_H__ */
