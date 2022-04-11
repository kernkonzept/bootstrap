/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "arch/arm/mpu_consts.h"

namespace Arm {

struct Mpu
{
  struct Region
  {
    unsigned long prbar, prlar;

    struct Prot {
      enum {
        NX = 1U << 0,
        EL0 = 1U << 1,
        RO = 1U << 2,
      };
    };

    struct Type {
      // See respective MAIR definitions in mpu_consts.h
      enum {
        Mask      = 7U << 1,
        Normal    = MPU_ATTR_NORMAL,
        Device    = MPU_ATTR_DEVICE,
        Buffered  = MPU_ATTR_BUFFERED,
      };
    };

    inline void attr(unsigned long prot, unsigned long type)
    {
      prbar = (prbar & ~0x3fUL) | prot;
      prlar = (prlar & ~0x3fUL) | type;
    }

    Region() = default;

    Region(unsigned long start, unsigned long end, unsigned long p,
           unsigned long t)
    : prbar(start & ~0x3fUL), prlar(end & ~0x3fUL)
    { attr(p, t); }
  };

  static void init()
  {
    // HPRENR
    asm volatile("mcr p15, 4, %0, c6, c1, 1" : : "r"(0));
    asm volatile("mcr p15, 4, %0, c10, c2, 0" : : "r"(MAIR0_BITS));
    asm volatile("mcr p15, 4, %0, c10, c2, 1" : : "r"(MAIR1_BITS));
  }

  static unsigned long regions()
  {
    unsigned long v;
    asm volatile("mrc p15, 4, %0, c0, c0, 4" : "=r"(v));
    return v;
  }

  static void prbar(unsigned long v)
  { asm volatile("mcr p15, 4, %0, c6, c3, 0" : : "r"(v)); }

  static unsigned long prlar()
  {
    unsigned long v;
    asm volatile("mrc p15, 4, %0, c6, c3, 1" : "=r"(v));
    return v;
  }

  static void prlar(unsigned long v)
  { asm volatile("mcr p15, 4, %0, c6, c3, 1" : : "r"(v)); }

  static void prselr(unsigned long v)
  {
    asm volatile(
      "mcr p15, 4, %0, c6, c2, 1  \n"
      "isb                        \n"
      : : "r"(v));
  }

  static void set(unsigned i, Region const &r)
  {
    prselr(i);
    prbar(r.prbar);
    prlar(r.prlar | 1UL);
  }

  static void clear(unsigned i)
  {
    prselr(i);
    prlar(prlar() & ~1UL);
  }

  static void enable()
  {
    unsigned long hsctlr;

    asm("mrc p15, 4, %0, c1, c0, 0" : "=r"(hsctlr));
    hsctlr |= 1U;
    asm volatile("mcr p15, 4, %0, c1, c0, 0" : : "r"(hsctlr));
  }

  static void disable()
  {
    unsigned long hsctlr;

    asm("mrc p15, 4, %0, c1, c0, 0" : "=r"(hsctlr));
    hsctlr &= ~1UL;
    asm volatile("mcr p15, 4, %0, c1, c0, 0" : : "r"(hsctlr));
  }
};

}
