/*!
 * \file
 * \brief  Support for Renesas Gen3
 *
 * \date   2016-2017
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2016-2017 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_sh.h>
#include "support.h"
#include "startup.h"

namespace {
class Platform_arm_rcar3 : public Platform_base,
                           public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0xe6e88000;
    kuart.reg_shift    = 0;
    kuart.base_baud    = 14745600;
    kuart.baud         = 115200;
    kuart.irqno        = 196;
    static L4::Uart_sh _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    mem_manager->ram->add(Region(0x048000000, 0x07fffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x600000000, 0x63fffffff, ".ram", Region::Ram));
  }
};
}

REGISTER_PLATFORM(Platform_arm_rcar3);
