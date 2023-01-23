/*
 * Copyright (C) 2018-2022 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"
#include "panic.h"
#include "platform_dt-arm.h"
#include "qemu_fw_cfg.h"
#include "qemu_ramfb.h"

extern char _start;

namespace {
class Platform_arm_virt : public Platform_dt_arm
{
  bool probe() override { return true; }

  void init() override
  {
    // set defaults for reg_shift and baud_rate
    kuart.baud      = 115200;
    kuart.reg_shift = 0;

    kuart.base_address = 0x09000000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 33;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);

    /* This is a QEMU special. QEMU copies the FDT to the start of the
     * RAM but does not provide the address in case the payload is
     * started as ELF.
     * Further, Fiasco wants to have this part of the RAM, so copy the FDT
     * somewhere else. */
    void *fdt = (void *)RAM_BASE;
    if (fdt_check_header(fdt) == 0)
      {
        _Pragma("GCC diagnostic push")
        _Pragma("GCC diagnostic ignored \"-Wnonnull\"") // if RAM_BASE == 0
        _Pragma("GCC diagnostic ignored \"-Warray-bounds\"") // if RAM_BASE == 0
        unsigned s = fdt_totalsize(fdt);
        unsigned long dst = l4_trunc_page((unsigned long)&_start - s);
        memmove((void *)dst, fdt, s);
        _Pragma("GCC diagnostic pop")
#if defined(ARCH_arm64)
        boot_args.r[0] = dst;
#elif defined(ARCH_arm)
        boot_args.r[2] = dst;
#endif
      }
  }

  void setup_fw_cfg()
  {
    Dt::Node fw_cfg = dt.node_by_compatible("qemu,fw-cfg-mmio");
    l4_uint64_t fw_cfg_addr;
    if (fw_cfg.is_valid() && fw_cfg.get_reg(0, &fw_cfg_addr))
      Fw_cfg::init_mmio(fw_cfg_addr);
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Platform_dt::construct_mbi(mod_addr, mods);
    setup_fw_cfg();
    setup_ramfb(mbi);
    return mbi;
  }

  void late_setup(l4_kernel_info_t *kip) override
  {
    bool have_smmuv3 = dt.node_by_compatible("arm,smmu-v3").is_valid();
    bool kernel_uses_smmuv3 = l4_kip_kernel_has_feature(kip, "arm,smmu-v3");

    if (kernel_uses_smmuv3 && !have_smmuv3)
      panic("Error: Microkernel uses SMMU-v3 but QEMU missing '-M virt,iommu=smmuv3'");
  }

  void reboot() override
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_arm_virt);
