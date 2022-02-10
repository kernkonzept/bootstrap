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

  static inline void dc_isw(unsigned long v)
  {
    asm volatile("mcr p15, 0, %0, c7, c6, 2" : : "r" (v) : "memory");
  }

  static inline void ic_iallu()
  {
    asm volatile("mcr p15, 0, r0, c7, c5, 0" : : : "memory");
  }

  static inline unsigned long get_ccsidr(unsigned long csselr)
  {
    unsigned long ccsidr;
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr));
    Barrier::isb();
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
    return ccsidr;
  }

  static bool hyp_mode()
  {
    unsigned long cpsr;
    asm("mrs %0, cpsr" : "=r"(cpsr));
    return (cpsr & 0x1fU) == 0x1aU;
  }

  static unsigned long sctlr()
  {
    unsigned long sctlr;

    if (hyp_mode())
      asm("mrc p15, 4, %0, c1, c0, 0" : "=r"(sctlr)); // HSCTLR
    else
      asm("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr)); // SCTLR

    return sctlr;
  }

  static void sctlr(unsigned long sctlr)
  {
    if (hyp_mode())
      asm volatile("mcr p15, 4, %0, c1, c0, 0" : : "r"(sctlr)); // HSCTLR
    else
      asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr)); // SCTLR
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
  Barrier::dsb_system();
  asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr) : "memory");
  asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::inv()
{
  Arm_v7plus::set_way_full_loop(Arm::Internal::dc_isw,
                                Arm::Internal::get_clidr,
                                Arm::Internal::get_ccsidr,
                                Arm_v7plus::set_way_dcache_noinfo_op());
  Barrier::dsb_system();
}

void Cache::Data::inv(unsigned long addr)
{
  Barrier::dsb_system();
  asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r" (addr) : "memory");
  asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::flush()
{
  Arm_v7plus::set_way_full_loop(Arm::Internal::dc_cisw,
                                Arm::Internal::get_clidr,
                                Arm::Internal::get_ccsidr,
                                Arm_v7plus::set_way_dcache_noinfo_op());
  Barrier::dsb_system();
}

void Cache::Data::flush(unsigned long addr)
{
  Barrier::dsb_system();
  asm volatile("mcr p15, 0, %0, c7, c14, 1 \n"
               "mcr p15, 0, %0, c7, c5, 1  \n"
               : : "r" (addr) : "memory");
  asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory");
  Barrier::dsb_system();
}

bool Cache::Data::enabled()
{
  return Arm::Internal::sctlr() & (1UL << 2);
}

void Cache::Data::enable()
{
  Barrier::dsb_system();
  Arm::Internal::sctlr(Arm::Internal::sctlr() | (1UL << 2));
  Barrier::dsb_system();
  Barrier::isb();
}

void Cache::Data::disable()
{
  Barrier::dsb_system();
  Arm::Internal::sctlr(Arm::Internal::sctlr() & ~(1UL << 2));
  Barrier::dsb_system();
  Barrier::isb();
}

void Cache::Insn::enable()
{
  asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0)); // ICIALLU
  Barrier::dsb_cores();
  Barrier::isb();
  Arm::Internal::sctlr(Arm::Internal::sctlr() | (1UL << 12));
  Barrier::isb();
}

void Cache::Insn::disable()
{
  Arm::Internal::sctlr(Arm::Internal::sctlr() & ~(1UL << 12));
  Barrier::isb();
  Barrier::dsb_cores();
  asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0)); // ICIALLU
  Barrier::dsb_cores();
  Barrier::isb();
}
