/*!
 * \file
 * \brief  Support for NXP LX2160
 *
 * \date   2020
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2020 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include <l4/sys/compiler.h>
#include "support.h"
#include "platform_dt-arm.h"
#include "startup.h"

namespace {

class Platform_arm_lx2160 : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    kuart.base_address = 0x21c0000;
    kuart.base_baud    = 0;
    kuart.irqno        = 64;
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  enum
  {
    MC_GCR1 = 0x00, // General Control Register 1

    MC_GCR1_P1_STOP = 1UL << 31, // Processor 1 Stop
    MC_GCR1_P2_STOP = 1UL << 30, // Processor 2 Stop
  };

  static l4_uint64_t read_a72_cpuactlr_el1()
  {
    l4_umword_t v;
    if constexpr (sizeof(long) == 4)
      asm volatile ("mrrc p15, 0, %Q0, %R0, c15" : "=r"(v));
    else
      asm volatile ("mrs %0, S3_1_c15_c2_0" : "=r"(v));
    return v;
  }

  void late_setup(l4_kernel_info_t *) override
  {
    dt.check_for_dt();

    // Pause the DPAA2 management complex (MC) firmware to avoid faults once
    // Fiasco enables the IOMMU. If then later someone wants to use the MC, they
    // have to resume it.
    l4_uint64_t mc_addr;
    Dt::Node mc = dt.node_by_compatible("fsl,qoriq-mc");
    if (!mc.is_valid() || !mc.get_reg(1, &mc_addr))
      {
        printf("  Could not find and pause DPAA2 management complex.\n");
        return;
      }

    L4::Io_register_block_mmio mc_reg(mc_addr);
    mc_reg.set<l4_uint32_t>(MC_GCR1, MC_GCR1_P1_STOP | MC_GCR1_P2_STOP);
    printf("  Paused DPAA2 management complex.\n");

    if (!(read_a72_cpuactlr_el1() & (1ULL << 31)))
      printf("  WARNING: CPUACTLR_EL1[31] is not set -- see corresponding LX2160 erratum!\n");
  }

  void reboot() override
  {
    reboot_psci();
    l4_infinite_loop();
  }
};
}

REGISTER_PLATFORM(Platform_arm_lx2160);
