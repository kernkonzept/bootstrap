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
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/drivers/uart_sh.h>
#include "support.h"
#include "startup.h"
#include "platform_dt-arm.h"

namespace {
class Platform_arm_rcar3 : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0xe6e88000;
    kuart.reg_shift    = 0;
    kuart.base_baud    = 14745600;
    kuart.baud         = 115200;
    kuart.irqno        = 196;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    set_uart_compatible(&kuart, "renesas,scif");
    static L4::Uart_sh _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void late_setup(l4_kernel_info_t *kip) override
  {
    set_dtb_in_kip(kip);
  }

  void reboot() override
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_arm_rcar3);
