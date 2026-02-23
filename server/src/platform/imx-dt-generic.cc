/*
 * Copyright (C) 2024-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#ifdef CONFIG_DRIVERS_FRST_UART_DRV_IMX
#include <l4/drivers/uart_imx.h>
#endif
#ifdef CONFIG_DRIVERS_FRST_UART_DRV_LPUART
#include <l4/drivers/uart_lpuart.h>
#endif
#include "platform_dt-arm.h"
#include "startup.h"

namespace {

class Platform_arm_imx_dt_generic : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    dt.check_for_dt();
    Dt::Node node = dt.get_stdout_uart(nullptr, parse_gic_irq, &kuart, &kuart_flags);
    if (!node.is_valid())
      return;

    static L4::Io_register_block_mmio r(kuart.base_address);

    L4::Uart *_uart = nullptr;
#ifdef CONFIG_DRIVERS_FRST_UART_DRV_IMX
    // imx8mp
    if (node.check_compatible("fsl,imx8mp-uart"))
      {
        static L4::Uart_imx8 uart;
        set_uart_compatible(&kuart, "fsl,imx8mp-uart");
        _uart = &uart;
      }
#endif
#ifdef CONFIG_DRIVERS_FRST_UART_DRV_LPUART
    // imx95
    // imx8pm
    if (!_uart && node.check_compatible("fsl,imx95-lpuart")
               || node.check_compatible("fsl,imx8qm-lpuart"))
      {
        static L4::Uart_lpuart uart(kuart.base_baud);
        set_uart_compatible(&kuart, "fsl,imx8qm-lpuart");
        _uart = &uart;
      }
#endif

    if (_uart)
      {
        _uart->startup(&r);
        set_stdio_uart(_uart);
      }
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

REGISTER_PLATFORM(Platform_arm_imx_dt_generic);
