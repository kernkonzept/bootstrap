/*
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *            Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/drivers/uart_pl011.h>
#include <l4/drivers/uart_16550.h>

#include "acpi.h"
#include "efi-support.h"
#include "panic.h"
#include "platform-arm.h"
#include "platform_dt-arm.h"
#include "startup.h"
#include "support.h"

namespace {

char const * const psci_methods[] = { "unsupported", "SMC", "HVC" };

class Platform_arm_sbsa : public Platform_dt_arm
{
  unsigned _uart_variant;

  void set_compatible_from_uart_variant(unsigned variant)
  {
    switch (variant)
      {
      case Acpi::Spcr::Arm_pl011:
      case Acpi::Spcr::Bcm2835:
        set_uart_compatible(&kuart, "arm,pl011");
        break;
      case Acpi::Spcr::Arm_sbsa:
      case Acpi::Spcr::Arm_sbsa_32bit:
        set_uart_compatible(&kuart, "arm,sbsa-uart");
        break;
      case Acpi::Spcr::Arm_dcc:
        set_uart_compatible(&kuart, "arm,dcc");
        break;
      case Acpi::Spcr::Nsc16550:
      case Acpi::Spcr::Compat16550:
        set_uart_compatible(&kuart, "ns16550");
        break;
      default:
        set_uart_compatible(&kuart, "l4re,uart-unknown");
        break;
      }
  }

public:
  bool probe() override { return true; }

  l4_addr_t get_fdt_addr() const override { return (unsigned long)efi.fdt(); }

  void init() override
  {
    // Prefer device tree over ACPI
    if (efi.fdt())
      {
        printf("SBSA with device tree\n");

        kuart.base_baud = 0;
        auto uart = dt.get_stdout_uart(nullptr, &Platform_dt_arm::parse_gic_irq,
                                       &kuart, &kuart_flags);

        if (!uart.is_valid())
          panic("EFI: no stdout UART found in device tree");

        if (   uart.check_compatible("arm,pl011")
            || uart.check_compatible("arm,primecell"))
          _uart_variant = Acpi::Spcr::Arm_pl011;
        else if (uart.check_compatible("arm,sbsa-uart"))
          _uart_variant = Acpi::Spcr::Arm_sbsa;
        else
          _uart_variant = Acpi::Spcr::Nsc16550;

        set_compatible_from_uart_variant(_uart_variant);

        query_psci_method();

        // Disable ACPI visibility for L4Re components, such as fiasco and io
        efi.disable_acpi();
      }
    else
      {
        Acpi::Sdt sdt;
        if (!sdt.init(efi.acpi_rsdp()))
          panic("No ACPI RSDP found!\n");
        else
          printf("SBSA with ACPI\n");

        auto *spcr = sdt.find<Acpi::Spcr const *>("SPCR");
        if (!spcr)
          panic("No ACPI SPCR found!");

        // For ACPI, we derive the compatible string from the SPCR's type,
        // as ACPI's UART choices are a subset of DT's possibilities, and
        // let the kernel choose via compatible_id.
        _uart_variant = spcr->type;
        set_compatible_from_uart_variant(_uart_variant);

        kuart.base_address = spcr->base.address;
        kuart.irqno = spcr->irq_gsiv;
        kuart.base_baud = 0;
        kuart.baud = spcr->baud_rate;
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart_flags       |=   L4_kernel_options::F_uart_base
                             | L4_kernel_options::F_uart_baud
                             | L4_kernel_options::F_uart_irq;

        printf("ACPI/SPCR UART: %s, addr=%llx type=%d irq=%d\n",
               kuart.compatible_id, kuart.base_address, _uart_variant,
               kuart.irqno);

        auto *fadt = sdt.find<Acpi::Fadt const *>("FACP");
        if (!fadt)
          panic("No ACPI FADT found!");

        if (fadt->arm_boot_flags & Acpi::Fadt::Psci_compliant)
          set_psci_method((fadt->arm_boot_flags & Acpi::Fadt::Psci_use_hvc)
                          ? Psci_hvc
                          : Psci_smc);
        printf("PSCI: %s\n", psci_methods[get_psci_method()]);
      }

    efi.setup_gop();
  }

  Boot_modules *modules() override { return this; }

  void setup_memory_map() override
  {
    if (efi.fdt())
      mem_manager->regions->add(
            Region::start_size(efi.fdt(), fdt_totalsize(efi.fdt()),
                               ".dtb", Region::Root));
    efi.setup_memory();
  }

  void late_setup(l4_kernel_info_t *kip) override
  {
    kip->acpi_rsdp_addr = reinterpret_cast<l4_umword_t>(efi.acpi_rsdp());
    kip->dt_addr = reinterpret_cast<l4_umword_t>(dt.fdt());
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &mods) override
  {
    l4util_l4mod_info *mbi = Boot_modules_image_mode::construct_mbi(mod_addr, mods);
    return efi.construct_mbi(mbi);
  }

  void firmware_announce_memory(Region n) override
  {
    efi.firmware_announce_memory(n);
  }

  void exit_boot_services() override
  {
    if (!(kuart_flags & L4_kernel_options::F_noserial)
        && _uart_variant != Acpi::Spcr::Arm_pl011
        && _uart_variant != Acpi::Spcr::Nsc16550
        && _uart_variant != Acpi::Spcr::Arm_sbsa_32bit
        && _uart_variant != Acpi::Spcr::Arm_sbsa)
      panic("EFI: unsupported uart type: %d", _uart_variant);

    efi.exit_boot_services();

    if (kuart_flags & L4_kernel_options::F_noserial)
      return;

    // EFI console is gone. Use our own UART driver from now on...
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_16550 _uart_16550(kuart.base_baud);
    static L4::Uart_pl011 _uart_pl011(kuart.base_baud);

    L4::Uart *u = &_uart_pl011;
    if (_uart_variant == Acpi::Spcr::Nsc16550)
      u = &_uart_16550;

    u->startup(&r);
    set_stdio_uart(u);
  }

  void reboot() override
  {
    reboot_psci();
  }
};
}

REGISTER_PLATFORM(Platform_arm_sbsa);
