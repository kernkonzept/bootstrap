#pragma once

void Cache::Data::clean()
{
  asm volatile("1:  mrc p15, 0, r15, c7, c14, 3 \n" // write back data cache
               "    bne 1b\n"
               "    mcr p15, 0, %0, c7, c10, 4  \n" // drain write buffer
               : : "r" (0) : "memory");
}

void Cache::Data::clean(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr));
}

void Cache::Data::flush(unsigned long addr)
{
  asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r" (addr));
  Barrier::dsb_system();
}

void Cache::Data::inv(unsigned long addr)
{
  asm volatile("mcr  p15, 0, %0, c7, c6, 1" : : "r" (addr));
}

bool Cache::Data::enabled()
{
  unsigned long r;
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r) : : "memory");
  return r & (1 << 2);
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

void Barrier::dsb_system()
{
  asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory");
}

void Barrier::dsb_cores()
{
  dsb_system();
}

void Barrier::dmb_system()
{
  asm volatile("" : : : "memory");
}

void Barrier::dmb_cores()
{
  asm volatile("" : : : "memory");
}

void Barrier::isb()
{
  asm volatile("" : : : "memory");
}
