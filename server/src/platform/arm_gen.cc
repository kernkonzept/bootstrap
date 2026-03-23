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

        l4_uint8_t shift;
        if (uart->reg_shift(&shift))
          kuart.reg_shift = shift;
        static L4::Io_register_block_mmio r(kuart.base_address,
                                            kuart.reg_shift);
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

    // We need to start an iterator here over all "drivers" we know and call.
    setup_plat_lx2160a();
  }

  void reboot() override
  {
    reboot_psci();

    if (dt.have_fdt())
      {
        // QCOM: Fallback for older platforms without PSCI support in the
        // firmware
        l4_uint64_t addr;
        Dt::Node pshold = dt.node_by_compatible("qcom,pshold");
        if (pshold.is_valid() && pshold.get_reg(0, &addr))
          *reinterpret_cast<volatile l4_uint32_t *>(addr) = 0;
      }
  }

  // NXP LX2160
  static l4_uint64_t read_a72_cpuactlr_el1()
  {
    l4_umword_t v;
    if constexpr (sizeof(long) == 4)
      asm volatile ("mrrc p15, 0, %Q0, %R0, c15" : "=r"(v));
    else
      asm volatile ("mrs %0, S3_1_c15_c2_0" : "=r"(v));
    return v;
  }

  // Checking for "fsl,qoriq-mc"
  void setup_plat_lx2160a()
  {
    // Pause the DPAA2 management complex (MC) firmware to avoid faults once
    // Fiasco enables the IOMMU. If then later someone wants to use the MC, they
    // have to resume it.
    enum
    {
      MC_GCR1 = 0x00, // General Control Register 1

      MC_GCR1_P1_STOP = 1UL << 31, // Processor 1 Stop
      MC_GCR1_P2_STOP = 1UL << 30, // Processor 2 Stop
    };

    Dt::Node mc = dt.node_by_compatible("fsl,qoriq-mc");
    l4_uint64_t mc_addr;
    if (mc.is_valid() && mc.get_reg(1, &mc_addr))
      {
        L4::Io_register_block_mmio mc_reg(mc_addr);
        mc_reg.set<l4_uint32_t>(MC_GCR1, MC_GCR1_P1_STOP | MC_GCR1_P2_STOP);
        printf("  Paused DPAA2 management complex.\n");

        if (!(read_a72_cpuactlr_el1() & (1ULL << 31)))
          printf("  WARNING: CPUACTLR_EL1[31] is not set -- see corresponding LX2160 erratum!\n");
      }
  }
};
}

REGISTER_PLATFORM(Platform_arm_gen);
