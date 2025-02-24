#pragma once

#include "mem_v7plus.h"

namespace Arm
{
class Internal
{
public:
  static inline unsigned long get_clidr()
  {
    unsigned long clidr;
    asm volatile("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr));
    return clidr;
  }

  static inline void dc_cisw(unsigned long v)
  {
    asm volatile("mcr p15, 0, %0, c7, c14, 2" : : "r" (v) : "memory");
  }

  static inline void dc_csw(unsigned long v)
  {
    asm volatile("mcr p15, 0, %0, c7, c10, 2" : : "r" (v) : "memory");
  }

  static inline void ic_iallu()
  {
    asm volatile("ic iallu" : : : "memory");
  }

  static inline unsigned long get_ccsidr(unsigned long csselr)
  {
    unsigned long ccsidr;
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr));
    Barrier::isb();
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
    return ccsidr;
  }

  static unsigned linesize_bytes()
  {
    return 1 << ((get_ccsidr(0 /* L1 data or unified */) & 7) + 4);
  }

};

} // namespace Arm

void Cache::Data::clean()
{
  Arm_v7plus::set_way_full_loop(Arm::Internal::dc_csw,
                                Arm::Internal::get_clidr,
                                Arm::Internal::get_ccsidr,
                                Arm_v7plus::set_way_dcache_noinfo_op());
  Barrier::dsb_system();
}

void Cache::Data::clean(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr) : "memory"); // DCCMVAC
  Barrier::dsb_system();
}

void Cache::Data::clean(unsigned long start, unsigned long size)
{
  unsigned cl_size = Arm::Internal::linesize_bytes();
  unsigned long m = start & ~(cl_size - 1);
  unsigned long e = (start + size + cl_size - 1) & (cl_size - 1);
  asm volatile("" : : : "memory");
  for (; m != e; m += cl_size)
    asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (m));
  Barrier::dsb_system();
}

void Cache::Data::inv(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r" (addr) : "memory"); // DCIMVAC
  Barrier::dsb_system();
}

void Cache::Data::flush(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r" (addr) : "memory"); // DCCIMVAC
  Barrier::dsb_system();
}

bool Cache::Data::enabled()
{
  unsigned long r;
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r));
  return r & (1UL << 2);
}

void Cache::Data::disable()
{
  unsigned long r;
  Barrier::dsb_system();
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r));
  r &= 1 << 2;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");
}

void Cache::Insn::disable()
{
  unsigned long r;
  asm ("mrc p15, 0, %0, c1, c0, 0" : "=r" (r));
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r & ~(1UL << 12)));
  Barrier::isb();
  Barrier::dsb_cores();
  asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0)); // ICIALLU
  Barrier::isb();
  Barrier::dsb_cores();
}

void Cache::Insn::inv()
{
  asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory"); // ICIALLU
  Barrier::dsb_system();
  Barrier::isb();
}
