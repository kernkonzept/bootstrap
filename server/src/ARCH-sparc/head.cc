#include <l4/sys/compiler.h>

#include "platform.h"
#include "support.h"
#include "startup.h"

extern "C" void __main();
void __main()
{
  clear_bss();
  ctor_init();
  Platform_base::iterate_platforms();

  init_modules_infos();
  startup(mod_header->mbi_cmdline());
  l4_infinite_loop();
}
