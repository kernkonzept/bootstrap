#pragma once

#include <stdio.h>

void Barrier::dsb_system()
{
  asm volatile("dsb sy");
}

void Barrier::dsb_cores()
{
  asm volatile("dsb ish");
}

void Barrier::dmb_system()
{
  asm volatile("dmb sy");
}

void Barrier::dmb_cores()
{
  asm volatile("dmb ish");
}

void Barrier::isb()
{
  asm volatile("isb");
}

class Arm_v7plus
{
public:
  struct set_way_dcache_info_op
  {
    void operator()(l4_addr_t cl, l4_addr_t a, l4_addr_t w, l4_addr_t s, l4_addr_t log) const
    { printf("cl=%ld assoc=%ld shift=%ld set=%ld log=%ld\n", cl, a, w, s, log); }
  };

  struct set_way_dcache_noinfo_op
  {
    void operator()(l4_addr_t, l4_addr_t, l4_addr_t, l4_addr_t, l4_addr_t) const {}
  };

  template< typename T, typename CLIDR, typename CCSIDR, typename D >
  static inline void
  set_way_full_loop(T const &set_way_fn,
                    CLIDR const &clidr_fn,
                    CCSIDR const &ccsidr_fn,
                    D const &info = set_way_dcache_noinfo_op())
  {
    Barrier::dmb_cores();
    unsigned long clidr = clidr_fn();
    unsigned lvl = (clidr >> 23) & 14;

    for (unsigned cl = 0; cl < lvl; cl += 2)
      {
        if (((clidr >> (cl + (cl / 2))) & 6) == 0)
          continue;

        unsigned long ccsidr = ccsidr_fn(cl);

        unsigned assoc       = ((ccsidr >> 3) & 0x3ff);
        unsigned w_shift     = __builtin_clz(assoc);
        unsigned set         = ((ccsidr >> 13) & 0x7fff);
        unsigned log2linelen = (ccsidr & 7) + 4;

        info(cl, assoc, w_shift, set, log2linelen);

        do
          {
            unsigned w = assoc;
            do
              set_way_fn((w << w_shift) | (set << log2linelen) | cl);
            while (w--);
          }
        while (set--);
      }

    Barrier::dsb_cores();
    Barrier::isb();
  }
};
