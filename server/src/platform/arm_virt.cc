/*
 * Copyright (C) 2018-2024 Kernkonzept GmbH.
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

  l4_addr_t get_fdt_addr() const override
  {
    if (dt.have_fdt())
      return reinterpret_cast<l4_addr_t>(dt.fdt());

    /* This is a QEMU special: QEMU copies the FDT to the start of the RAM but
     * does not provide the address in case the payload is started as ELF.
     * Fiasco wants to have this part of the RAM, so copy the FDT. */
    void const *fdt = reinterpret_cast<void const *>(0x4000'0000);

    // Fall back to the platform methods (used for example for uimage)
    if (fdt_check_header(fdt) != 0)
      {
        fdt = reinterpret_cast<void const *>(Platform_dt_arm::get_fdt_addr());

        // Fail if this one was also not valid
        if (fdt_check_header(fdt) != 0)
          return 0;
      }

    unsigned sz = fdt_totalsize(fdt);
    l4_addr_t dst = l4_trunc_page(reinterpret_cast<unsigned long>(&_start) - sz);
    memmove((void *)dst, fdt, sz);
    return dst;
  }

  void init() override
  {
    kuart.baud   = 115200;
    kuart_flags |= L4_kernel_options::F_uart_baud;

    dt.check_for_dt();
    dt.get_stdout_uart("arm,pl011", &parse_gic_irq, &kuart, &kuart_flags);

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
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
