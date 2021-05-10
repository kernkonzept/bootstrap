/*!
 * \file
 * \brief  Support for ARM's AEM FVP Base platform
 *
 * \date   2021
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */


#include <assert.h>
#include <l4/drivers/uart_pl011.h>

#include "support.h"
#include "startup.h"

namespace {

class Platform_arm_fvp_base : public Platform_base,
                              public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_baud    = 24019200;
    kuart.base_address = 0x1c090000;
    kuart.baud         = 115200;
    kuart.irqno        = 37;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    // FVP is a virtual platform, so be flexible wrt our memory
    // configuration
    unsigned size_mb = RAM_SIZE_MB;
    assert(size_mb <= 4096);
    mem_manager->ram->add(Region(0x080000000,
                                 0x080000000
                                 + ((size_mb < 2048 ? size_mb : 2048) << 20),
                                 ".ram", Region::Ram));
    if (size_mb > 2048)
      mem_manager->ram->add(Region(0x880000000,
                                   0x880000000 + ((size_mb - 2048) << 20),
                                   ".ram", Region::Ram));
  }
};

}

REGISTER_PLATFORM(Platform_arm_fvp_base);
