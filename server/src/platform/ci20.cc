/*
 * \brief  Support for the CI20 platform
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <l4/drivers/uart_16550.h>
#include <l4/sys/compiler.h>
#include "support.h"
#include "platform-mips.h"
#include "macros.h"
#include "startup.h"
#include "mips-defs.h"

namespace {

class Platform_mips_ci20 : public Platform_single_region_ram<Platform_mips>
{
public:
  void init() override
  {
    kuart.base_baud    = 3000000;
    kuart.reg_shift    = 2;

    switch (PLATFORM_UART_NR)
      {
      case 0:
        kuart.base_address = 0x10030000;
        kuart.irqno        = 51;
        break;
      case 1:
        kuart.base_address = 0x10031000;
        kuart.irqno        = 50;
        break;
      case 2:
        kuart.base_address = 0x10032000;
        kuart.irqno        = 49;
        break;
      case 3:
        kuart.base_address = 0x10033000;
        kuart.irqno        = 48;
        break;
      default:
      case 4:
        kuart.base_address = 0x10034000;
        kuart.irqno        = 34;
        break;
      }

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

  void setup_memory_map() override
  {
    mem_manager->ram->add(Region(0x0,        0x0fffffff, ".ram", Region::Ram));
    // Note, the first 256MB are mirrored at 0x20000000
    mem_manager->ram->add(Region(0x30000000, 0x5fffffff, ".ram", Region::Ram));
  }

  void reboot() override
  {
    printf("MIPS CI20 reboot not implemented\n");
    l4_infinite_loop();
  }

  const char *get_platform_name()
  { return "ci20"; }
};

}

REGISTER_PLATFORM(Platform_mips_ci20);
