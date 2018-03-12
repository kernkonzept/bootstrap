/*
 * \brief  Support for the CI20 platform
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <l4/drivers/uart_16550.h>
#include "support.h"
#include "macros.h"
#include "startup.h"
#include "mips-defs.h"

namespace {

class Platform_mips_ci20 : public Platform_single_region_ram
{
public:
 bool probe()
  {
    return true;
  }

  void init()
  {
    unsigned long uart_base;

    kuart.base_baud    = 3000000;
    kuart.base_address = 0x10034000; // UART4
    kuart.reg_shift    = 2;
    kuart.irqno        = 34;

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0x10 /* FCR UME */);
    static L4::Io_register_block_mmio r(kuart.base_address + Mips::KSEG1,
                                        kuart.reg_shift);

    _uart.startup(&r);
    _uart.change_mode(L4::Uart_16550::MODE_8N1, 115200);
    set_stdio_uart(&_uart);

    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart.baud         = 115200;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud;
    kuart_flags |= L4_kernel_options::F_uart_irq;
  }

  void setup_memory_map()
  {
    // let the base class scan for RAM
    Platform_single_region_ram::setup_memory_map();

    // reserve @ 0x0000       : first page containing exception base
    // reserve @ 0x0f00-0x1000: SMP cpulaunch structures to prevent launch flags
    // being zeroed by bootstrap -presetmem=0 command line arguement
    // mem_manager->regions->add(Region::n(0, L4_PAGESIZE, ".kernel_resv",
    //                                    Region::Kernel));
  }

  l4_uint64_t to_phys(l4_addr_t bootstrap_addr)
  { return bootstrap_addr - Mips::KSEG0; }

  l4_addr_t to_virt(l4_uint64_t phys_addr)
  { return phys_addr + Mips::KSEG0; }

  void reboot()
  {
    printf("MIPS CI20 reboot not implemented\n");
    for (;;)
      ;
  }

  const char *get_platform_name()
  { return "ci20"; }
};

}

REGISTER_PLATFORM(Platform_mips_ci20);
