#pragma once

void Cache::Data::clean()
{
  asm volatile("mcr p15, 0, %0, c7, c10, 0" : : "r" (0) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::clean(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::clean(unsigned long start, unsigned long size)
{
  unsigned cl_size = 32;
  unsigned long m = start & ~(cl_size - 1);
  unsigned long e = (start + size + cl_size - 1) & ~(cl_size - 1);
  asm volatile("" : : : "memory");
  for (; m != e; m += cl_size)
    asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (m));
  Barrier::dsb_system();
}

void Cache::Data::flush(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r" (addr) : "memory");
  Barrier::dsb_system();
}

void Cache::Data::inv(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r" (addr) : "memory");
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
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r));
  r &= 1 << 2;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");
}

void Cache::Insn::disable()
{
  unsigned long r;
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r));
  r &= 1 << 12;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");
}

void Cache::Insn::inv()
{
  asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory");
  Barrier::dsb_system();
  Barrier::isb();
}

void Barrier::dsb_system()
{
  asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); // CP15DSB
}

void Barrier::dsb_cores()
{
  dsb_system();
}

void Barrier::dmb_system()
{
  asm volatile("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory"); // CP15DMB
}

void Barrier::dmb_cores()
{
  dmb_system();
}

void Barrier::isb()
{
  asm volatile("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory"); // CP15ISB
}
