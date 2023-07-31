/*
 * Copyright (C) 2015, 2020-2021, 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org
 *            Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#define BOOTSTRAP_MOD_INFO_MAGIC_HDR "<< L4Re-bootstrap-modinfo-hdr >>"
#define BOOTSTRAP_MOD_INFO_MAGIC_MOD "<< L4Re-bootstrap-modinfo-mod >>"

/*
 * Flags for a module
 *
 * Bits 0..2: Type of module
 *            0:   unspecified, any module
 *            1:   kernel
 *            2:   sigma0
 *            3:   roottask
 *            4-7: reserved
 */

enum Mod_info_flags
{
  Mod_info_flag_mod_unspec    = 0,
  Mod_info_flag_mod_kernel    = 1,
  Mod_info_flag_mod_sigma0    = 2,
  Mod_info_flag_mod_roottask  = 3,
  Mod_info_flag_mod_mask      = 7 << 0,
};

enum Mod_header_flags
{
};

class Mod_base
{
protected:
  template<typename T>
  T* rel2abs(unsigned long long v) const
  { return reinterpret_cast<T*>(reinterpret_cast<l4_addr_t>(this) + v); }

  template<typename T>
  unsigned long long abs2rel(T *v)
  {
    return reinterpret_cast<unsigned long long>(v)
            - reinterpret_cast<unsigned long long>(this);
  }
};

/// Info for each module
class Mod_info : private Mod_base
{
  struct { // avoid clang warnings about unused fields
    char _magic[32];
    unsigned long long _flags;

    unsigned long long _start;
    unsigned _size;
    unsigned _size_uncompressed;
    unsigned long long _name;
    unsigned long long _cmdline;
    unsigned long long _md5sum_compr;
    unsigned long long _md5sum_uncompr;
    unsigned long long _attrs; // always relative to this structure
  };
public:
  inline unsigned size() const
  { return _size; }

  inline void size(unsigned size)
  { _size = size; }

  inline unsigned size_uncompressed() const
  { return _size_uncompressed; }
  enum { Num_base_modules = 3 };

  char const* name()           const { return rel2abs<char>(_name);           }
  char const* cmdline()        const { return rel2abs<char>(_cmdline);        }
  char const* start()          const { return rel2abs<char>(_start);          }
  char const* md5sum_compr()   const { return rel2abs<char>(_md5sum_compr);   }
  char const* md5sum_uncompr() const { return rel2abs<char>(_md5sum_uncompr); }

  void start(unsigned long long start)
  { _start = start; }

  void start(char const *addr)
  { _start = abs2rel(addr); }

  Mod_info_flags flags() const
  { return Mod_info_flags(_flags); }

  bool is_base_module() const
  {
    unsigned v = _flags & Mod_info_flag_mod_mask;
    return v > 0 && v <= Num_base_modules;
  }


  inline bool compressed() const
  { return _size != _size_uncompressed; }

} __attribute__((packed)) __attribute__((aligned(8)));

class Mod_info_list
{
  Mod_info *_mods;
  unsigned _num_mods;

public:
  Mod_info_list(Mod_info *mods, unsigned num_mods)
  : _mods(mods), _num_mods(num_mods)
  {}

  Mod_info const *begin() const { return _mods; }
  Mod_info *begin() { return _mods; }

  Mod_info const *end() const { return _mods + _num_mods; }
  Mod_info *end() { return _mods + _num_mods; }

  Mod_info const *operator[](unsigned i) const { return _mods + i; }
  Mod_info *operator[](unsigned i) { return _mods + i; }
};

class Mod_header : private Mod_base
{
  struct { // avoid clang warnings about unused private fields
    char _magic[32];
    unsigned _num_mods;
    unsigned _flags;
    unsigned long long _mbi_cmdline;
    unsigned long long _mods;
  };

public:
  inline unsigned num_mods() const
  { return _num_mods; }

  inline Mod_info_list mods()
  { return Mod_info_list(rel2abs<Mod_info>(_mods), _num_mods); }

  inline char const *mbi_cmdline() const
  { return rel2abs<char>(_mbi_cmdline); }
} __attribute__((packed)) __attribute__((aligned(8)));
