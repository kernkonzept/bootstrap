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

/// Info for each module
class Mod_info
{
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

  inline char const *
  abs(unsigned long long v) const
  {
    return (char const *)((unsigned long)this + v);
  }

  inline unsigned long long rel(char const *v)
  {
    return (unsigned long long)v - (unsigned long long)this;
  }

  //static char const *to_phys(char const *addr);
  //static char const *to_virt(char const *addr);

public:
  inline unsigned long long flags() const
  { return _flags; }

  inline unsigned size() const
  { return _size; }

  inline void size(unsigned size)
  { _size = size; }

  inline unsigned size_uncompressed() const
  { return _size_uncompressed; }

  inline char const *cmdline() const
  { return abs(_cmdline); }

  inline char const *name() const
  { return abs(_name); }

  inline char const *start() const
  { return abs(_start); }

  inline void start(char const *addr)
  { _start = rel(addr); }

  inline char const *md5sum_compr() const
  { return abs(_md5sum_compr); }

  inline char const *md5sum_uncompr() const
  { return abs(_md5sum_uncompr); }

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

class Mod_header
{
  char _magic[32];
  unsigned _num_mods;
  unsigned _flags;
  unsigned long long _mbi_cmdline;
  unsigned long long _mods;

  inline char const *
  hdr_offs_to_charp(unsigned long long offset) const
  {
    return (char const *)((char const *)this + offset);
  }

public:
  inline unsigned num_mods() const
  { return _num_mods; }

  inline Mod_info_list mods() const
  {
    return Mod_info_list((Mod_info *)hdr_offs_to_charp(_mods), _num_mods);
  }

  inline char const *
  mbi_cmdline()
  {
    return hdr_offs_to_charp(_mbi_cmdline);
  }
} __attribute__((packed)) __attribute__((aligned(8)));
