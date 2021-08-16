#pragma once

#ifdef CTEST
#include "l4int.h"
#else
#include <l4/sys/l4int.h>
#endif

namespace Cache {

class Data
{
public:
  static inline void clean();
  static inline void clean(unsigned long);
  static inline void inv(unsigned long);
  static inline void flush(unsigned long);

  static inline bool enabled();
  static inline void disable();
};

class Insn
{
public:
  static inline void disable();
};

} // namespace Cache

class Barrier
{
public:
  static inline void dsb_system();
  static inline void dsb_cores();
  static inline void dmb_system();
  static inline void dmb_cores();
  static inline void isb();
};

#if __ARM_ARCH >= 8
#if __SIZEOF_POINTER__ == 8
#include "mem_v8_64.h"
#else
#include "mem_v8_32.h"
#endif
#elif __ARM_ARCH == 7
#include "mem_v7.h"
#elif __ARM_ARCH == 6
#include "mem_v6.h"
#elif __ARM_ARCH == 5
#include "mem_v5.h"
#else
#error Unknown/unsupported Arm architecture variant
#endif
