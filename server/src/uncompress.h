/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#ifndef __BOOTSTRAP__UNCOMPRESS_H__
#define __BOOTSTRAP__UNCOMPRESS_H__

void *decompress(const char *name, const char *start, char *destbuf,
                 int size, int size_uncompressed);

#endif /* ! __BOOTSTRAP__UNCOMPRESS_H__ */
