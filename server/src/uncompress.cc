/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Marcus Haehnel marcus.haehnel@kernkonzept.com
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file	bootstrap/server/src/uncompress.cc
 * \brief	Support for on-the-fly uncompressing of boot modules
 * 
 * \date	2004
 * \author	Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <l4/sys/l4int.h>
#include <l4/sys/consts.h>

#define ZLIB_CONST
#include <zlib.h>

#include "startup.h"
#include "uncompress.h"

alignas(long) static char static_buf[16 << 10];
static char* free_ptr = &static_buf[0];

void free(void * /*address*/)
{}

void* malloc(size_t size)
{
  unsigned long remaining = &static_buf[sizeof(static_buf)] - free_ptr;
  size = (size + sizeof(long) - 1U) & (sizeof(long) - 1U);
  if (size > remaining)
    {
      printf("Cannot alloc %lu bytes, only %lu available\n", size, remaining);
      return nullptr;
    }

  void *current = free_ptr;
  free_ptr += size;
  return current;
}

void *
decompress(const char *name, const char *start, char *destbuf,
           int size, int size_uncompressed)
{
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = size;
  strm.next_in = reinterpret_cast<z_const Bytef *>(start);
  strm.avail_out = size_uncompressed;
  strm.next_out = reinterpret_cast<Bytef *>(destbuf);

  int ret = inflateInit2(&strm, 31);
  if (ret != Z_OK)
    {
      printf("Failed to initialize inflate: %i\n", ret);
      return NULL;
    }

  printf("  Uncompressing %s from %p to %p (%d to %d bytes, %+lld%%).\n",
        name, start, destbuf, size, size_uncompressed,
        100*(unsigned long long)size_uncompressed/size - 100);

  ret = inflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END)
    {
      printf("Failed to decompress: %i\n", ret);
      return NULL;
    }

  if (strm.avail_out != 0)
    {
      printf("Incorrect decompression: should be %d bytes but got %d bytes.\n",
             size_uncompressed, size_uncompressed - strm.avail_out);
      return NULL;
    }

  return destbuf;
}
