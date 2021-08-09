/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "dt.h"
#include "platform_riscv.h"
#include "startup.h"
#include "support.h"

#include <l4/drivers/uart_sbi.h>

void Platform_riscv_base::init()
{
  static L4::Uart_sbi _uart;
  set_stdio_uart(&_uart);
}

l4_addr_t Platform_riscv_base::get_fdt_addr() const
{
  return boot_args.r[1];
}

void Platform_riscv_base::init_dt(Internal_module_list &mods)
{
  Platform_dt<Platform_base>::init_dt(mods);
  dt.check_for_dt();

  // Now that the device tree is available, set up the kernel UART.
  setup_kuart();
}

void Platform_riscv_base::setup_kernel_config(l4_kernel_info_t*kip)
{
  l4_kip_platform_info_arch &arch_info = kip->platform_info.arch;
  if (!parse_isa_ext(arch_info))
    Dt::warn("Failed to parse ISA extensions.\n");

  if (!parse_harts(arch_info))
    Dt::warn("Failed to parse harts.\n");

  arch_info.timebase_frequency = get_timebase_frequency();

  if (!parse_interrupt_target_contexts(arch_info))
    Dt::warn("Failed to parse interrupt target contexts.\n");
}

void Platform_riscv_base::boot_kernel(unsigned long entry)
{
  // Notify other harts that kernel is ready to boot
  asm volatile ("fence" : : : "memory");
  extern volatile unsigned long mp_launch_boot_kernel;
  mp_launch_boot_kernel = entry;

  Platform_base::boot_kernel(entry);
}

void Platform_riscv_base::setup_kuart_from_dt(char const *compatible)
{
  kuart.access_type  = L4_kernel_options::Uart_type_mmio;
  kuart_flags       |=   L4_kernel_options::F_uart_base
                       | L4_kernel_options::F_uart_baud
                       | L4_kernel_options::F_uart_irq;

  // Use UART specified via /chosen/stdout-path if present and compatible
  auto chosen = dt.node_by_path("/chosen");
  if (chosen.is_valid())
    {
      const char *uart_path = chosen.get_prop_str("stdout-path");
      if (uart_path && parse_kuart_node(dt.node_by_path(uart_path), compatible))
        return;
    }

  // Otherwise scan all compatible UARTs
  bool found_uart = false;
  dt.nodes_by_compatible(compatible, [&](Dt::Node uart)
    {
      if (parse_kuart_node(uart, compatible))
        {
          found_uart = true;
          return Dt::Break;
        }
      return Dt::Continue;
    });

  if (!found_uart)
    Dt::warn("Failed to setup kernel uart!\n");
}

bool Platform_riscv_base::parse_kuart_node(Dt::Node uart,
                                           char const *compatible)
{
  if (!uart.is_valid() || !uart.is_enabled())
    return false;

  if (!uart.check_compatible(compatible))
    return false;

  l4_uint32_t base_baud;
  if (uart.get_prop_u32("clock-frequency", base_baud))
    kuart.base_baud = base_baud;
  else
    uart.info("Failed to get uart clock frequency!\n");

  l4_uint32_t baud;
  if (uart.get_prop_u32("current-speed", baud))
    kuart.baud = baud;
  else
    uart.info("Failed to get uart current speed!\n");

  l4_uint64_t base_address;
  if (uart.get_reg(0, &base_address))
    kuart.base_address = base_address;
  else
    uart.info("Failed to get uart base address!\n");

  l4_uint32_t irq;
  if (uart.get_prop_u32("interrupts", irq))
    kuart.irqno = irq;
  else
    uart.info("Failed to get uart irq!\n");

  l4_uint32_t reg_shift;
  if (uart.get_prop_u32("reg-shift", reg_shift))
    kuart.reg_shift = reg_shift;

  return true;
}

char const *Platform_riscv_base::riscv_cpu_isa(Dt::Node cpu) const
{
  if (!cpu.check_device_type("cpu"))
    // Not a cpu node.
    return nullptr;

  if (!cpu.check_compatible("riscv"))
    // Incompatible cpu.
    return nullptr;

  if (!cpu.is_enabled())
    // Cpu is not available.
    return nullptr;

  return cpu.get_prop_str("riscv,isa");
}

bool
Platform_riscv_base::riscv_cpu_hartid(Dt::Node cpu, l4_uint32_t &hartid) const
{
  char const *isa = riscv_cpu_isa(cpu);
  if (!isa)
    // Cpu without riscv,isa property.
    return false;

  if (strlen(isa) < 2 || isa[0] != 'r' || isa[1] != 'v')
    // Cpu with invalid isa.
    return false;

  // Return hart id.
  return cpu.get_prop_u32("reg", hartid);
}

bool
Platform_riscv_base::parse_isa_ext(l4_kip_platform_info_arch &arch_info) const
{
  l4_uint32_t isa_ext = 0;

  Dt::Node cpus = dt.node_by_path("/cpus");
  if(!cpus.is_valid())
    // Failed to find cpus node.
    return false;

  cpus.for_each_subnode([&](Dt::Node cpu)
    {
      char const *isa = riscv_cpu_isa(cpu);
      if (!isa)
        // Not a cpu node.
        return; // continue

      if (!strncmp(isa, "rv32", 4) || !strncmp(isa, "rv64", 4))
        // Skip rv32/rv64 prefix.
        isa += 4;
      else
        // Cpu does not have a valid riscv isa.
        return; // continue

      l4_uint32_t tmp_isa_ext = 0;
      for (size_t i = 0; i < strlen(isa); i++)
        {
          // Z (unprivileged)/S (privileged)/X (vendor) extensions follow
          // separated by underscores.
          if (isa[i] == '_')
            break;

          // Standard extensions are mapped to the first 26 bits.
          char c = isa[i];
          if(c >= 'a' && c <= 'z')
            tmp_isa_ext |= 1 << (c - 'a');
        }

      // Merge with isa_ext of the other cpus (common subset).
      if(isa_ext == 0)
        isa_ext = tmp_isa_ext;
      else
        isa_ext &= tmp_isa_ext;
    });

  if(isa_ext == 0)
    // No cpu with a valid riscv isa was found.
    return false;

  arch_info.isa_ext = isa_ext;
  return true;
}

bool
Platform_riscv_base::parse_harts(l4_kip_platform_info_arch &arch_info) const
{
  l4_uint32_t max_hart_num = sizeof(arch_info.hart_ids)
                             / sizeof(arch_info.hart_ids[0]);
  l4_uint32_t hart_num = 0;

  Dt::Node cpus = dt.node_by_path("/cpus");
  if(!cpus.is_valid())
    // Failed to find cpus node.
    return false;

  cpus.for_each_subnode([&](Dt::Node cpu)
    {
      l4_uint32_t hartid;
      if (!riscv_cpu_hartid(cpu, hartid))
        return; // continue

      if(hart_num < max_hart_num)
          arch_info.hart_ids[hart_num++] = hartid;
      else
        Dt::warn("Hart %u exceeds maximum number of harts.\n", hartid);
    });

  if(hart_num == 0)
    return false;

  arch_info.hart_num = hart_num;
  return true;
}

l4_uint32_t Platform_riscv_base::get_timebase_frequency() const
{
  l4_uint32_t timebase_frequency = 0;

  Dt::Node cpus = dt.node_by_path("/cpus");
  if (cpus.is_valid())
    cpus.get_prop_u32("timebase-frequency", timebase_frequency);

  if (!timebase_frequency)
    Dt::warn("Failed to get timebase-frequency.");

  return timebase_frequency;
}

bool
Platform_riscv_base::parse_interrupt_target_contexts(l4_kip_platform_info_arch &arch_info) const
{
  // TODO: Platform should provide compatible property for PLIC.
  Dt::Node plic = dt.node_by_compatible("riscv,plic0");
  if (!plic.is_valid())
    plic = dt.node_by_compatible("sifive,plic-1.0.0");
  if (!plic.is_valid())
    plic = dt.node_by_compatible("migv,plic");
  if (!plic.is_valid())
    // Failed to find PLIC.
    return false;

  l4_uint64_t plic_addr;
  if (!plic.get_reg(0, &plic_addr))
    // Failed to get PLIC address.
    return false;

  arch_info.plic_addr = plic_addr;

  plic.get_prop_u32("riscv,ndev", arch_info.plic_nr_irqs);

  auto cells = plic.get_prop_array("interrupts-extended", { 1, 1 });
  if (!cells.is_valid())
    // Failed to read interrupt targets.
    return false;

  for (unsigned context_nr = 0; context_nr < cells.elements(); context_nr++)
    {
      auto target_handle = cells.get<l4_uint32_t>(context_nr, 0);
      Dt::Node ic = dt.node_by_phandle(target_handle);
      if (!ic.is_valid())
        // Failed to resolve interrupt controller.
        return false;

      if (!ic.has_prop("interrupt-controller"))
        // Not an interrupt controller.
        return false;

      l4_uint32_t ic_cells;
      if (!ic.get_prop_u32("#interrupt-cells", ic_cells))
        // No #interrupt-cells property.
        return false;

      if (ic_cells != 1)
        // Unsupported #interrupt-cells count.
        return false;

      if (cells.get<l4_uint32_t>(context_nr, 1) == -1u)
        // Skip invalid targets.
        continue;

      if (!ic.check_compatible("riscv,cpu-intc"))
        // Not a cpu interrupt controller.
        continue;

      Dt::Node hart = ic.parent_node();
      if (!hart.is_valid())
        // Failed to resolve parent.
        continue;

      l4_uint32_t hartid;
      if (!riscv_cpu_hartid(hart, hartid))
        // Hart has no hartid or is not valid.
        continue;

      // Find position for hartid
      int hart_pos = -1;
      for (unsigned h = 0; h < arch_info.hart_num; h++)
        {
          if (arch_info.hart_ids[h] == hartid)
            {
              hart_pos = h;
              break;
            }
        }

      if (hart_pos == -1)
        {
          hart.warn("Hart %u not in the list of detected harts.\n", hartid);
          continue;
        }

      arch_info.plic_hart_irq_targets[hart_pos] = context_nr;
      hart.info("Hart %u is assigned to interrupt target context %u.\n",
               hartid, context_nr);
    }

  return true;
}
