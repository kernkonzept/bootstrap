/*
 * \brief  Support for the Ci40 platform
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

class Platform_mips_ci40 : public Platform_single_region_ram<Platform_mips>
{
public:
  void init() override
  {
    unsigned long uart_base;

    kuart.base_baud    = 115740;
    kuart.reg_shift    = 2;
    kuart.base_address = 0x18101500; // UART1
    kuart.irqno        = 25; // GIC-25

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

  void reboot() override
  {
    L4::Io_register_block_mmio wdg(Mips::KSEG1 + 0x18102100);
    wdg.write(0, 1);
    l4_infinite_loop();
  }

  const char *get_platform_name()
  { return "Ci40"; }
};

}

REGISTER_PLATFORM(Platform_mips_ci40);
