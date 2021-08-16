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
    asm volatile("mrs %0, CLIDR_EL1" : "=r" (clidr));
    return clidr;
  }

  static inline void dc_cisw(unsigned long v)
  {
    asm volatile("dc cisw, %0" : : "r" (v) : "memory");
  }

  static inline void dc_csw(unsigned long v)
  {
    asm volatile("dc csw, %0" : : "r" (v) : "memory");
  }

  static inline void ic_iallu()
  {
    asm volatile("ic iallu" : : : "memory");
  }

  static inline unsigned long get_ccsidr(unsigned long csselr)
  {
    unsigned long ccsidr;
    asm volatile("msr CSSELR_EL1, %0" : : "r" (csselr));
    Barrier::isb();
    asm volatile("mrs %0, CCSIDR_EL1" : "=r" (ccsidr));
    return ccsidr;
  }

  static unsigned current_el()
  {
    unsigned long c_el;
    asm ("mrs %0, CurrentEL" : "=r" (c_el));
    return (c_el >> 2) & 3;
  }

  static unsigned long sctlr()
  {
    unsigned long sctlr;
    switch (current_el())
      {
#if __ARM_ARCH_PROFILE != 82
      case 3:
        asm ("mrs %0, SCTLR_EL3" : "=r"(sctlr));
        break;
#endif
      case 2:
        asm ("mrs %0, SCTLR_EL2" : "=r"(sctlr));
        break;
      case 1:
        asm ("mrs %0, SCTLR_EL1" : "=r"(sctlr));
        break;
      default:
        sctlr = 0;
      }
    return sctlr;
  }

  static void sctlr(unsigned long sctlr)
  {
    switch (current_el())
      {
#if __ARM_ARCH_PROFILE != 82
      case 3:
        asm ("msr SCTLR_EL3, %0" : : "r"(sctlr));
        break;
#endif
      case 2:
        asm ("msr SCTLR_EL2, %0" : : "r"(sctlr));
        break;
      case 1:
        asm ("msr SCTLR_EL1, %0" : : "r"(sctlr));
        break;
      default:
        break;
      }
  }
};
}


bool Cache::Data::enabled()
{
  return Arm::Internal::sctlr() & (1 << 2);
}

void Cache::Data::disable()
{
  Barrier::dsb_system();
  Arm::Internal::sctlr(Arm::Internal::sctlr() & ~(1 << 2));
}

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
  asm volatile("dc cvac, %0" : : "r" (addr) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::inv(unsigned long addr)
{
  Barrier::dsb_system();
  asm volatile("dc ivac, %0" : : "r" (addr) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::flush(unsigned long addr)
{
  Barrier::dsb_system();
  asm volatile("dc civac, %0" : : "r" (addr) : "memory");
  Barrier::dsb_system();
}

void Cache::Insn::disable()
{
  unsigned long sctlr;
  asm ("mrs %0, SCTLR_EL2" : "=r"(sctlr) : : "memory");
  asm ("msr SCTLR_EL2, %0" : : "r"(sctlr & ~(1UL << 12)) : "memory");
  Barrier::isb();
  Barrier::dsb_cores();
  asm volatile("ic ialluis" : : : "memory");
  Barrier::isb();
  Barrier::dsb_cores();
}
