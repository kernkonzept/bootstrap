/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef REGION_H
#define REGION_H

#include <l4/sys/compiler.h>
#include <l4/sys/kip>
#include <l4/sys/l4int.h>
#include <assert.h>

#include "types.h"

/** Region in memory. */
class Region
{
public:
  enum Type { No_mem, Kernel, Sigma0, Boot, Root, Arch, Ram, Info };

  enum Subtype_info
  {
    Info_acpi_rsdp = L4::Kip::Mem_desc::Info_acpi_rsdp,

    Arch_acpi = L4::Kip::Mem_desc::Arch_acpi_tables,
    Arch_nvs = L4::Kip::Mem_desc::Arch_acpi_nvs,

    /** Regions that are reserved early and discarded before any elf loading */
    Boot_temporary = 5,
  };

  /** Basic noop constructor, to be able to have array without ini code */
  Region() = default;

  /** Create an invalid region. */
  Region(Type) : Region(0ULL) {};

  /** Create a 1byte region at begin, basically for lookups */
  Region(unsigned long long begin)
  : _begin(begin), _end(begin), _name(0), _t(No_mem), _s(0), _eager(false)
  {}

  /** Create a 1byte region for address \a ptr.
   * @param ptr the start address for the 1byte region.
   */
  Region(void const *ptr)
  : _begin(reinterpret_cast<l4_addr_t>(ptr)),
    _end(reinterpret_cast<l4_addr_t>(ptr)), _name(0), _t(No_mem), _s(0),
    _eager(false)
  {}

  /** Create a fully fledged region.
   * @param begin The start address.
   * @param end The address of the last byte in the region.
   * @param name The name for the region (usually the binary name).
   * @param t The type of the region.
   * @param sub The subtype of the region.
   * @param eager Mark region as eagerly mapped by kernel
   */
  Region(unsigned long long begin, unsigned long long end,
         char const *name = 0, Type t = No_mem, short sub = 0,
         bool eager = false)
  : _begin(begin), _end(end), _name(name), _t(t), _s(sub), _eager(eager)
  {
    assert(_begin <= _end);
  }

  /**
   * Create a region ...
   * @param other a region to copy the name, the type, and the sub_type from
   * @param begin the start address of the new region
   * @param end the end address (inclusive) of the new region
   */
  Region(Region const &other, unsigned long long begin, unsigned long long end)
  : _begin(begin), _end(end), _name(other._name), _t(other._t), _s(other._s),
    _eager(other._eager)
  {
    assert(_begin <= _end);
  }

  /**
   * Create a region from start and size.
   * @param begin the start address of the region
   * @param size the size of the region in bytes
   * @param name the name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   * @param eager Mark region as eagerly mapped by kernel
   */
  static Region start_size(unsigned long long begin, unsigned long long size,
                           char const *name = 0, Type t = No_mem,
                           short sub = 0, bool eager = false)
  {
    assert(size > 0);
    return Region(begin, begin + size - 1, name, t, sub, eager);
  }

  /**
   * Create a region from start and size.
   * @param begin the start address of the region
   * @param size the size of the region in bytes
   * @param name the name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   * @param eager Mark region as eagerly mapped by kernel
   */
  static Region start_size(void const *begin, unsigned long long size,
                           char const *name = 0, Type t = No_mem,
                           short sub = 0, bool eager = false)
  {
    return start_size(reinterpret_cast<l4_addr_t>(begin), size, name, t, sub,
                      eager);
  }

  /**
   * Create a region for the given object.
   * @param begin a pointer to the object that shall be covered
   *              (the size of the region will be sizeof(T))
   * @param name the name of the region
   * @param t the type of theregion
   * @param sub the subtype of the region
   * @param eager Mark region as eagerly mapped by kernel
   */
  template< typename T >
  static Region from_ptr(T const *begin, char const *name = 0,
                         Type t = No_mem, short sub = 0, bool eager = false)
  { return Region::start_size(reinterpret_cast<l4_addr_t>(begin), sizeof(T),
                              name, t, sub, eager); }

  /**
   * Create a region for an array of objects.
   * @param begin pointer to the first element of the array
   * @param size the number of elements in the array (the size of the
   *             whole region will be size * sizeof(T))
   * @param name the name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   * @param eager Mark region as eagerly mapped by kernel
   */
  template< typename T >
  static Region array(T const *begin, unsigned long size, char const *name = 0,
                      Type t = No_mem, short sub = 0, bool eager = false)
  {
    return Region::start_size(reinterpret_cast<l4_addr_t>(begin),
                              sizeof(T) * size, name, t, sub, eager);
  }

  /** Get the start address. */
  unsigned long long begin() const { return _begin; }
  /** Get the address of the last byte. */
  unsigned long long end() const { return _end; }

  /** Set the start address. */
  void begin(unsigned long long begin)
  {
    _begin = begin;
    assert(_begin <= _end);
  }

  /** Set the address of the last byte. */
  void end(unsigned long long end)
  {
    _end = end;
    assert(_begin <= _end);
  }

  /** Get the name of the region. */
  char const *name() const { return _name; }
  /** Get size of the region */
  unsigned long long size() const { return _end - _begin + 1; }
  /** Set the name of the region. */
  void name(char const *name) { _name = name; }
  /** Get the type of the region. */
  Type type() const { return static_cast<Type>(_t); }
  /** Set the type of the region. */
  void type(Type t) { _t = t; }
  /** Get the subtype of the region. */
  short sub_type() const { return _s; }
  /** Set the subtype of the region. */
  void sub_type(short s) { _s = s; }

  /** Print the region [begin; end] */
  void print() const;

  /** Print the region verbose (with name and type). */
  void vprint() const;

  /** Compare two regions. */
  bool operator < (Region const &o) const
  { return end() < o.begin(); }

  /** Check for an overlap. */
  bool overlaps(Region const &o) const
  { return !(*this < o) && !(o < *this); }

  /** Test if o is a sub-region of ourselves. */
  bool contains(Region const &o) const
  { return begin() <= o.begin() && end() >= o.end(); }

  /** Calculate the intersection. */
  Region intersect(Region const &o) const
  {
    if (!overlaps(o))
      return Region(No_mem);

    return Region(begin() > o.begin() ? begin() : o.begin(),
                  end() < o.end() ? end() : o.end(),
                  name(), type(), sub_type());
  }

  /** Check if the region is invalid */
  bool invalid() const { return begin()==0 && end()==0; }

  /** Get eager mapping flag. */
  bool eager() const { return _eager; }

private:
  unsigned long long _begin, _end;
  char const *_name;
  unsigned char _t, _s;
  bool _eager;
};


/** List of memory regions, based on an fixed size array. */
class Region_list
{
public:
  /**
   * Initialize the region list, using the array of given size
   * as backing store.
   */
  void init(Region *store, unsigned size,
            const char *name,
            unsigned long long max_combined_size = ~0ULL,
            unsigned long long address_limit = ~0ULL)
  {
    _reg = _end = store;
    _max = _reg + size;
    _name = name;
    _max_combined_size = max_combined_size;
    _address_limit = address_limit;
    _combined_size = 0;
  }

  /** Helper template for array parameters
   *
   * This helper allows to access the type and the size of compile-time
   * bound arrays. In particular used as parameters.
   */
  template< typename T >
    struct Array_type_helper;

  template< typename T, unsigned SZ >
    struct Array_type_helper<T[SZ]> {
      enum { size = SZ };
      typedef T type;
    };

  /**
   * Initialize the region list, using a region array as store
   *
   * @param store the array for the regoion backing store.
   * @param name the name of the region list for output
   * @param max_combined_size max sum of bytes in this list
   * @param address_limit maximum allowed address in this list
   */
  template<typename STORE>
  void init(STORE &store,
            const char *name,
            unsigned long long max_combined_size = ~0ULL,
            unsigned long long address_limit = ~0ULL)
  {
    init(store, Array_type_helper<STORE>::size, name,
         max_combined_size, address_limit);
  }

  /** Search for a region that overlaps o. */
  Region *find(Region const &o) const;

  /** Search for the region that contains o. */
  Region *contains(Region const &o);

  /**
   * Search for a memory region not overlapping any known region,
   * within search.
   */
  unsigned long long find_free(Region const &search,
                               unsigned long long size, unsigned align) const;

  /**
   * Search for a memory region not overlapping any know region, starting
   * from the end of the search region.
   */
  unsigned long long find_free_rev(Region const &search, unsigned long long _size,
                                   unsigned align) const;
  /**
   * Add a new region, with a upper limit check and verboseness.
   */
  void add(Region const &r, bool may_overlap = false);

  bool sub(Region const &r);

  /** Dump the whole region list. */
  void dump();

  /** Get the begin() iterator. */
  Region *begin() const { return _reg; }
  /** Get the end() iterator. */
  Region *end() const { return _end; }

  /** Remove the region given by the iterator r. */
  Region *remove(Region *r);

  /** Optimize the region list.
   * Basically merges all regions with the same type, subtype, and name
   * that have a begin and end address on the same memory page.
   */
  void optimize();

protected:
  Region *_end;
  Region *_max;
  Region *_reg;

  const char *_name;
  unsigned long long _max_combined_size;
  unsigned long long _address_limit;
  unsigned long long _combined_size;

private:
  /**
   * Add a new memory region to the list. The new region must not overlap
   * any known region. The resulting list is sorted in ascending order.
   */
  void add_nolimitcheck(Region const &r, bool may_overlap = false);
};

#endif
