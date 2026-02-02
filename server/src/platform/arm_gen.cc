/*
 * Copyright (C) 2025
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "support.h"
#include "startup.h"
#include "platform_dt-arm.h"

extern char _start;

namespace {
class Platform_arm_gen : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    dt.check_for_dt();
    Dt::Node node = dt.get_stdout_uart(nullptr, &parse_gic_irq,
                                       &kuart, &kuart_flags);
    if (!node.is_valid())
      return;

    node.stringlist_for_each("compatible",
                             [&](unsigned, const char *c)
      {
        L4::Uart *uart
          = l4re_dev_uart_create_by_dt_compatible_once(c, kuart.base_baud);
        if (!uart)
          return Dt::Continue;

        static L4::Io_register_block_mmio r(kuart.base_address);
        uart->startup(&r);
        set_stdio_uart(uart);

        set_uart_compatible(&kuart, c);

        return Dt::Break;
      });

    query_psci_method();
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

REGISTER_PLATFORM(Platform_arm_gen);
