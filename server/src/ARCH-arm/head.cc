#include "support.h"
#include "startup.h"

extern "C" int __aeabi_unwind_cpp_pr0(void);
extern "C" int __aeabi_unwind_cpp_pr1(void);

enum { _URC_FAILURE  = 9 };
int __aeabi_unwind_cpp_pr0(void) { return _URC_FAILURE; }
int __aeabi_unwind_cpp_pr1(void) { return _URC_FAILURE; }

extern "C" void __main();
void __main()
{
  unsigned long r;

  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r) : : "memory");
  r &= ~1UL;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");

  clear_bss();
  ctor_init();
  Platform_base::iterate_platforms();

  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));
  while(1)
    ;
}
