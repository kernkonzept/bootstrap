/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021, 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include "dt.h"
#include "isa_parser.h"
#include "platform_riscv.h"
#include "startup.h"
#include "support.h"

#include <l4/drivers/uart_sbi.h>

void Platform_riscv_base::init()
{
}

l4_addr_t Platform_riscv_base::get_fdt_addr() const
{
  return boot_args.r[1];
}

void Platform_riscv_base::init_dt()
{
  static L4::Uart_sbi _uart;
  set_stdio_uart(&_uart);

  Platform_dt<Platform_base>::init_dt();
  dt.check_for_dt();

  // Now that the device tree is available, set up the kernel UART.
  setup_kuart();
}

int Platform_riscv_base::parse_plic_irq(Dt::Node node)
{
  Dt::Array_prop<1> interrupts = node.get_prop_array("interrupts", { 1 });
  if (!interrupts.is_valid() || interrupts.elements() < 1)
    return -1;

  return interrupts.get(0, 0);
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
  // Use UART specified via /chosen/stdout-path if present and compatible
  if (dt.get_stdout_uart(compatible, parse_plic_irq, &kuart, &kuart_flags).is_valid())
    return;

  // Otherwise scan all compatible UARTs
  bool found_uart = false;
  dt.nodes_by_compatible(compatible, [&](Dt::Node uart)
    {
      if (dt.parse_uart(uart, parse_plic_irq, &kuart, &kuart_flags).is_valid())
        {
          found_uart = true;
          return Dt::Break;
        }
      return Dt::Continue;
    });

  if (!found_uart)
    Dt::warn("Failed to setup kernel uart!\n");
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

struct Isa_ext_bitmap
{
  enum { Words = cxx::array_size(l4_kip_platform_info_arch().isa_ext) };
  l4_uint32_t bits[Words] = { 0 };

  void set(L4_riscv_isa_ext ext)
  { bits[ext / 32] |= 1 << (ext % 32); }

  bool empty() const
  {
    for (l4_uint32_t w : bits)
      if (w != 0)
        return false;
    return true;
  }

  Isa_ext_bitmap & operator &= (Isa_ext_bitmap const &o)
  {
    for (unsigned i = 0; i < cxx::array_size(bits); ++i)
      bits[i] &= o.bits[i];
    return *this;
  }
};

struct Isa_ext_def
{
  char const *name;
  L4_riscv_isa_ext bit;
};

/**
 * Defines mapping from RISC-V ISA extension name to the corresponding
 * L4_riscv_isa_ext ID.
 *
 * New entries should be added according to the following ordering rules
 * described in the RISC-V unprivileged specification. Entries within each group
 * shall be ordered alphabetically.
 * 1. Standard unprivileged extensions, starting with a "Z" followed by an
 *    alphabetical name, where the first letter following the "Z" conventionally
 *    indicates the most closely related alphabetical extension category. They
 *    should be ordered first by category, then alphabetically within a
 *    category.
 * 2. Standard supervisor-level extensions, starting with "Ss" or "Sv"
 *    (virtual-memory architecture extension).
 * 3. Standard hypervisor-level extensions, starting with "Sh".
 * 4. Standard machine-level extensions, starting with "Sm".
 * 5. Non-standard extensions, starting with a single "X".
 */
static constexpr Isa_ext_def isa_ext_def[] =
{
  { "sstc", L4_riscv_isa_ext_sstc },
};

bool
Platform_riscv_base::parse_isa_ext(l4_kip_platform_info_arch &arch_info) const
{
  Isa_ext_bitmap isa_ext;

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

      if (strncmp(isa, "rv32", 4) && strncmp(isa, "rv64", 4))
        // Cpu does not have a valid riscv isa.
        return; // continue

      Isa_parser isa_parser(isa + 4); // +4 is to skip the rv32/rv64 prefix
      Isa_ext_bitmap tmp_isa_ext;
      while (isa_parser.next_ext())
        {
          cxx::String const ext = isa_parser.ext();

          // Single-letter standard extensions are mapped to the first 26 bits.
          if (ext.len() == 1 && ext[0] >= 'a' && ext[0] <= 'z')
            {
              tmp_isa_ext.set(static_cast<L4_riscv_isa_ext>(ext[0] - 'a'));
              continue;
            }

          for (auto const &def : isa_ext_def)
            if (static_cast<size_t>(ext.len()) == strlen(def.name)
                && !strncasecmp(ext.start(), def.name, ext.len()))
              tmp_isa_ext.set(def.bit);
        }

      if (isa_parser.has_err())
        Dt::warn("Errors occurred while parsing ISA string: %s", isa);

      // Merge with isa_ext of the other cpus (common subset).
      if(isa_ext.empty())
        isa_ext = tmp_isa_ext;
      else
        isa_ext &= tmp_isa_ext;
    });

  if(isa_ext.empty())
    // No cpu with a valid riscv isa was found.
    return false;

  memcpy(arch_info.isa_ext, isa_ext.bits, cxx::array_size(arch_info.isa_ext));
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
