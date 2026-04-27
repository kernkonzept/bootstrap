#include "platform.h"
#include "support.h"

#ifdef RAM_SIZE_MB
void
setup_single_region_ram_memory_map()
{
  unsigned long ram_base{RAM_BASE};
  unsigned long ram_size_mb{RAM_SIZE_MB};
  printf("  Memory size is %lu MiB (%08lx - %08lx)\n",
         ram_size_mb, ram_base, ram_base + (ram_size_mb << 20) - 1);
  mem_manager->ram->add(
      Region::start_size(ram_base, ram_size_mb << 20, ".ram", Region::Ram));
}
#endif
