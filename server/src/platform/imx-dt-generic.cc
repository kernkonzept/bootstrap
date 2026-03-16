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
    if (!_uart && (node.check_compatible("fsl,imx95-lpuart")
                   || node.check_compatible("fsl,imx8qm-lpuart")))
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

  /**
   * Make sure the kernel is loaded to a physical address similar to Linux
   * loaded by u-boot.
   * During PSCI calls, the ATF ensures that passed addresses follow certain
   * rules, e.g. for CPU_ON, the entry point must be >= PLAT_NS_IMAGE_OFFSET.
   */
  void setup_memory_map() override
  {
    Platform_dt_arm::setup_memory_map();

    Dt::Node root = dt.node_by_path("/");
    if (root.is_valid())
      {
        root.stringlist_for_each("compatible", [](unsigned, const char *c)
          {
            // Note that the first RAM region does not necessarily start at
            // 0xX0000000!
            unsigned long long min_base;
            if (starts_with(c, "fsl,imx8m"))
              min_base = 0x40200000;
            else if (starts_with(c, "fsl,imx8q"))
              min_base = 0x80020000; // no typo!
            else if (starts_with(c, "fsl,imx93"))
              min_base = 0x80200000;
            else if (starts_with(c, "fsl,imx94") || starts_with(c, "fsl,imx95"))
              min_base = 0x90200000;
            else
              return Dt::Continue;

            auto rsvd_start = mem_manager->ram->begin()->begin();
            auto rsvd_end = min_base - 1;
            if (rsvd_start < rsvd_end)
              mem_manager->regions->add(
                Region(rsvd_start, rsvd_end, "ATF-reserved", Region::Boot));

            return Dt::Break;
          });
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
