/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/drivers/uart_tegra-tcu.h>
#include "acpi.h"
#include "dt.h"
#include "efi-support.h"
#include "panic.h"
#include "platform-arm.h"
#include "platform_dt-arm.h"
#include "startup.h"
#include "support.h"

namespace {

class Platform_arm_nvidia_orin : public Platform_arm,
                                 public Boot_modules_image_mode
{
  struct Dt_module : Internal_module_base
  {
    Dt_module(unsigned long start, unsigned long size)
    : Internal_module_base(".fdt"), start(start), end(start + size)
    {}

    void set_region(l4util_l4mod_mod *m) const override
    {
      m->mod_start = start;
      m->mod_end   = end;
    }

    unsigned long start;
    unsigned long end;
  };

public:
  bool probe() override { return true; }

  void init() override
  {
    // The Orins can select between doing ACPI or DT. But ACPI does not seem
    // to be fully implemented.

    if (!efi.fdt())
      panic("L4Re needs to be booted with device tree!\n");
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    efi.setup_memory();
  }

  void late_setup(l4_kernel_info_t *kip) override
  {
    kip->dt_addr = reinterpret_cast<l4_umword_t>(_fdt);
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    // Contrary to the specification, the device tree is *not* contained in an
    // EfiACPIReclaimMemory region. To add insult to injury, the memory seems
    // to be reused. This means we have to copy the device tree before moving
    // on...
    Dt dt;
    dt.init(reinterpret_cast<unsigned long>(efi.fdt()));
    dt.check_for_dt();

    unsigned fdt_size = dt.fdt_size();
    _fdt = reinterpret_cast<void *>(mem_manager->find_free_ram(fdt_size));
    if (!_fdt)
      panic("fatal: could not allocate memory for DT\n");

    memcpy(_fdt, efi.fdt(), fdt_size);
    mem_manager->regions->add(Region::start_size(_fdt, fdt_size));

    Dt_module dt_module(reinterpret_cast<unsigned long>(_fdt), fdt_size);
    Internal_module_list all_mods(mods);
    all_mods.push_front(&dt_module);

    l4util_l4mod_info *mbi = Boot_modules_image_mode::construct_mbi(mod_addr, all_mods);
    return efi.construct_mbi(mbi);
  }

  void firmware_announce_memory(Region n) override
  {
    efi.firmware_announce_memory(n);
  }

  void exit_boot_services() override
  {
    efi.exit_boot_services();

    kuart.base_address = 0x0c168000;
    //kuart.irqno = 152; // this is a mbox-hsp doorbell irq!
    kuart.base_baud = 0;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    // disabled for now
    kuart_flags &= ~L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_tegra_tcu _uart;

    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot() override
  {
    reboot_psci();
  }

private:
  void *_fdt = nullptr;
};

}

REGISTER_PLATFORM(Platform_arm_nvidia_orin);
