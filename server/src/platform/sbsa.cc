/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *            Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/drivers/uart_pl011.h>
#include <l4/drivers/uart_16550.h>

#include "acpi.h"
#include "efi-support.h"
#include "panic.h"
#include "platform-arm.h"
#include "startup.h"
#include "support.h"

namespace {

char const * const psci_methods[] = { "unsupported", "SMC", "HVC" };

class Platform_arm_sbsa : public Platform_arm,
                          public Boot_modules_image_mode
{
  enum Psci_method { Psci_unsupported, Psci_smc, Psci_hvc };

public:
  bool probe() override { return true; }

  void init() override
  {
    Acpi::Sdt sdt;
    if (!sdt.init(efi.acpi_rsdp()))
      panic("No RSDP found!\n");

    auto *spcr = sdt.find<Acpi::Spcr const *>("SPCR");
    if (!spcr)
      panic("No SPCR found!");

    kuart.variant = spcr->type;
    kuart.base_address = spcr->base.address;
    kuart.irqno = spcr->irq_gsiv;
    kuart.base_baud = 0;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    auto *fadt = sdt.find<Acpi::Fadt const *>("FACP");
    if (!fadt)
      panic("No FADT found!");

    if (fadt->arm_boot_flags & Acpi::Fadt::Psci_compliant)
      _psci_method = (fadt->arm_boot_flags & Acpi::Fadt::Psci_use_hvc)
                      ? Psci_hvc
                      : Psci_smc;
    printf("PSCI: %s\n", psci_methods[_psci_method]);

    efi.setup_gop();
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    efi.setup_memory();
  }

  void late_setup(l4_kernel_info_t *kip) override
  {
    kip->acpi_rsdp_addr = reinterpret_cast<l4_umword_t>(efi.acpi_rsdp());
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Boot_modules_image_mode::construct_mbi(mod_addr, mods);
    return efi.construct_mbi(mbi);
  }

  void exit_boot_services() override
  {
    if (!(kuart_flags & L4_kernel_options::F_noserial)
        && kuart.variant != Acpi::Spcr::Arm_pl011
        && kuart.variant != Acpi::Spcr::Nsc16550
        && kuart.variant != Acpi::Spcr::Arm_sbsa_32bit
        && kuart.variant != Acpi::Spcr::Arm_sbsa)
      panic("EFI: unsupported uart type: %d", kuart.variant);

    efi.exit_boot_services();

    if (kuart_flags & L4_kernel_options::F_noserial)
      return;

    // EFI console is gone. Use our own UART driver from now on...
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_16550 _uart_16550(kuart.base_baud);
    static L4::Uart_pl011 _uart_pl011(kuart.base_baud);

    L4::Uart *u = &_uart_pl011;
    if (kuart.variant == Acpi::Spcr::Nsc16550)
      u = &_uart_16550;

    u->startup(&r);
    set_stdio_uart(u);
  }

  void reboot() override
  {
    register unsigned long r0 asm("r0") = 0x84000009;
    switch (_psci_method)
      {
      case Psci_smc: asm volatile ("smc #0" : : "r"(r0)); break;
      case Psci_hvc: asm volatile ("hvc #0" : : "r"(r0)); break;
      // We could probably support the reset-reg method too
      default: printf("Error: no PSCI support!\n"); break;
      }

    // should not be reached
    printf("PSCI reboot failed!\n");
    l4_infinite_loop();
  }

private:
  Psci_method _psci_method = Psci_unsupported;
};
}

REGISTER_PLATFORM(Platform_arm_sbsa);
