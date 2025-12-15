/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "panic.h"
#include <l4/util/l4_macros.h>
#include <l4/util/printf_helpers.h>
#include <l4/sys/consts.h>

#include "region.h"
#include "module.h"

unsigned long long
Region_list::find_free(Region const &search, unsigned long long _size,
                       unsigned align) const
{
  unsigned long long start = search.begin();
  unsigned long long end   = search.end();
  unsigned long long size  = l4_round_size(_size, align);
  while (1)
    {
      start = l4_round_size(start, align);

      if (start + size - 1 > end)
        return 0;

      if (0)
        printf("try start %p\n", reinterpret_cast<void *>(start));

      Region *z = find(Region::start_size(start, size));
      if (!z)
        return start;

      start = z->end() + 1;
    }
}

unsigned long long
Region_list::find_free_rev(Region const &search, unsigned long long _size,
                           unsigned align) const
{
  unsigned long long start = search.begin();
  unsigned long long end   = search.end();
  unsigned long long size  = l4_round_size(_size, align);
  while (1)
    {
      end = l4_trunc_size(end - size - 1, align);

      if (end < start)
        return 0;

      if (0)
        printf("try start %p\n", reinterpret_cast<void *>(end));

      Region *z = find(Region::start_size(end, size));
      if (!z)
        return end;

      end = z->begin() - 1;
    }
}

void
Region_list::add_nolimitcheck(Region const &region, bool may_overlap)
{
  /* Do not add empty regions */
  if (region.begin() == region.end())
    return;

  if (_end >= _max)
    {
      // try to merge adjacent regions to gain space
      optimize();

      if (_end >= _max)
        panic("Bootstrap: %s: Region overflow\n", __func__);
    }

  Region *r;
  if (begin() == end() || *(end() - 1) < region)
    r = end(); // optimized case: append region
  else
    for (r = begin(); r != end(); ++r)
      if (!(*r < region))
        {
          if (!may_overlap && !(region < *r))
            {
              printf("  New region for list %s:\t", _name);
              region.vprint();
              printf("  overlaps with:         \t");
              r->vprint();

              dump();
              panic("region overlap");
            }
          memmove(r + 1, r, (end() - r) * sizeof(Region));
          break;
        }

  *r = region;
  ++_end;
  _combined_size += region.size();
}

void
Region_list::add(Region const &region, bool may_overlap)
{
  Region mem = region;

  if (mem.invalid())
    {
      printf("  WARNING: trying to add invalid region to %s list.\n", _name);
      return;
    }

  if (mem.begin() > _address_limit)
    {
      printf("  Dropping '%s' region ", _name);
      mem.print();
      printf(" due to %llu MiB address limit\n", _address_limit >> 20);
      return;
    }

  if (mem.end() >= _address_limit)
    {
      printf("  Limiting '%s' region ", _name);
      mem.print();
      mem.end(_address_limit - 1);
      printf(" to ");
      mem.print();
      printf(" due to %llu MiB address limit\n", _address_limit >> 20);

    }

  if (_combined_size >= _max_combined_size)
    {
      printf("  Dropping '%s' region ", _name);
      mem.print();
      printf(" due to %llu MiB size limit\n", _max_combined_size >> 20);
      return;
    }

  if (_combined_size + mem.size() > _max_combined_size)
    {
      printf("  Limiting '%s' region ", _name);
      mem.print();
      mem.end(mem.begin() + _max_combined_size - _combined_size - 1);
      printf(" to ");
      mem.print();
      printf(" due to %llu MiB size limit\n", _max_combined_size >> 20);
    }

  add_nolimitcheck(mem, may_overlap);
}

Region *
Region_list::find(Region const &o) const
{
  for (Region *c = _reg; c < _end; ++c)
    if (c->overlaps(o))
      return c;

  return 0;
}

Region *
Region_list::contains(Region const &o)
{
  for (Region *c = _reg; c < _end; ++c)
    if (c->contains(o))
      return c;

  return 0;
}

void
Region::print(bool aligned) const
{
  char s[64];
  l4util_human_readable_size(s, sizeof(s), size());
  if (aligned)
    printf("  [%9llx, %9llx] {%10s}", begin(), end(), s);
  else
    printf("[%llx, %llx] {%s}", begin(), end(), s);
}

void
Region::vprint() const
{
  static char const *types[] = {"Gap   ", "Kern  ", "Sigma0",
                                "Boot  ", "Root  ", "Arch  ", "Ram   ",
                                "Info  " };
  printf("  ");
  print(true);
  printf(" %s ", types[type()]);
  if (name())
    {
      if (*name() == '.')
        printf("%s", name() + 1);
      else
        print_module_name(name(), "");
    }
  putchar('\n');
}

void
Region_list::dump()
{
  Region const *i;
  Region const *j;
  unsigned long long min, mark = 0;

  printf("Regions of list '%s'\n", _name);
  for (i = _reg; i < _end; ++i)
    {
      min = ~0;
      Region const *min_idx = 0;
      for (j = _reg; j < _end; ++j)
        if (j->begin() < min && j->begin() >= mark)
          {
            min     = j->begin();
            min_idx = j;
          }
      if (!min_idx)
        {
          i->vprint();
          continue;
        }
      min_idx->vprint();
      mark = min_idx->begin() + 1;
    }
}

Region *
Region_list::remove(Region *r)
{
  memmove(r, r+1, (end() - r - 1)*sizeof(Region));
  --_end;
  return r;
}

void
Region_list::optimize()
{
  Region *c = begin();
  while (c < end())
    {
      Region *n = c;
      ++n;
      if (n == end())
        return;

      if (n->type() == c->type() && n->sub_type() == c->sub_type()
          && n->name() == c->name() && n->eager() == c->eager()
          && l4_round_page(c->end()) >= l4_trunc_page(n->begin()))
        {
          c->end(n->end());
          remove(n);
        }
      else
        ++c;
    }
}

bool
Region_list::sub(Region const &r)
{
  Region *c = contains(r);
  if (!c)
    return false;

  if (c->begin() == r.begin() && c->end() == r.end())
    {
      remove(c);
      return true;
    }

  if (c->begin() == r.begin())
    {
      c->begin(r.end() + 1);
      return true;
    }

  if (c->end() == r.end())
    {
      c->end(r.begin() - 1);
      return true;
    }

  Region tail(*c);
  tail.begin(r.end() + 1);
  c->end(r.begin() - 1);
  add(tail);
  return true;
}
