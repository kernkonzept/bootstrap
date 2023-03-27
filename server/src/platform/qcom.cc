/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */

#include <l4/drivers/uart_dm.h>
#include <l4/drivers/uart_geni.h>
#include "startup.h"
#include "support.h"
#include "platform_dt-arm.h"

namespace {
class Platform_arm_qcom : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.baud         = 115200;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

#if defined(PLATFORM_TYPE_msm8226)
    kuart.base_address = 0xf991f000; // UART3
    kuart.irqno        = 32 + 109;
    static L4::Uart_dm _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_msm8909)
    kuart.base_address = 0x078af000; // UART1
    kuart.irqno        = 32 + 107;
    static L4::Uart_dm _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_msm8916) || defined(PLATFORM_TYPE_msm8939)
    kuart.base_address = 0x078b0000; // UART2
    kuart.irqno        = 32 + 108;
    static L4::Uart_dm _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_msm8974)
    kuart.base_address = 0xf991e000; // UART1
    kuart.irqno        = 32 + 107;
    static L4::Uart_dm _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_sm8150)
    kuart.base_address = 0x00a90000; // UART2
    kuart.irqno        = 32 + 357;
    static L4::Uart_geni _uart(kuart.base_baud);
#else
#error Unknown Qualcomm platform type
#endif

    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot() override
  {
    reboot_psci();

    // Fallback for older platforms without PSCI support in the firmware
    if (dt.have_fdt())
      {
        l4_uint64_t addr;
        Dt::Node pshold = dt.node_by_compatible("qcom,pshold");
        if (pshold.is_valid() && pshold.get_reg(0, &addr))
          *reinterpret_cast<volatile l4_uint32_t *>(addr) = 0;
        else
          printf("PSCI reboot failed and no 'qcom,pshold' address in DT.\n");
      }

    l4_infinite_loop();
  }
};
}

REGISTER_PLATFORM(Platform_arm_qcom);
