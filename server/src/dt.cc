/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <stdio.h>
#include <assert.h>

#include "panic.h"
#include "support.h"

#include "dt.h"

void Dt::init(unsigned long fdt_addr)
{
  if (!fdt_addr)
    return;

  _fdt = (void const *)fdt_addr;

  int fdt_check = fdt_check_header(_fdt);
  if (fdt_check < 0)
    {
      _fdt = nullptr;
      warn("FDT sanity check failed: %s (%d)\n",
           fdt_strerror(fdt_check), fdt_check);
      return;
    }
}

void Dt::check_for_dt() const
{
  if (!_fdt)
    panic("This platform needs a device tree, please provide one.");
}

Dt::Node Dt::node_by_path(char const *path, int namelen) const
{
  return Node(_fdt, fdt_path_offset_namelen(_fdt, path, namelen));
}

Dt::Node Dt::node_by_phandle(uint32_t phandle) const
{
  return Node(_fdt, fdt_node_offset_by_phandle(_fdt, phandle));
}

Dt::Node Dt::node_by_compatible(char const *compatible) const
{
  return Node(_fdt, fdt_node_offset_by_compatible(_fdt, -1, compatible));
}

char const *Dt::Node::get_prop_str(char const *name) const
{
  int len;
  char const *str = get_prop<char>(name, &len);
  // Ensure that the string is null-terminated as required by DTB spec.
  if (str && strnlen(str, len) == static_cast<size_t>(len))
    {
      warn("Property '%s' does not hold a valid string.", name);
      return nullptr;
    }
  return str;
}

bool Dt::Node::stringlist_contains(char const *name, char const *value) const
{
  return stringlist_search(name, value) >= 0;
}

int Dt::Node::stringlist_search(char const *name, char const *value) const
{
  return fdt_stringlist_search(_fdt, _off, name, value);
}

bool Dt::Node::get_addr_size_cells(unsigned &addr_cells,
                                   unsigned &size_cells) const
{
  int addr = fdt_address_cells(_fdt, _off);
  int size = fdt_size_cells(_fdt, _off);
  if (addr < 0 || addr > 2 || size < 0 || size > 2)
    {
      warn("Unsupported value for address/size cells: %d/%d\n", addr, size);
      return false;
    }
  addr_cells = addr;
  size_cells = size;
  return true;
}

bool Dt::Node::translate_reg(Node parent,
                             l4_uint64_t &addr, l4_uint64_t const &size) const
{
  if (parent.is_root_node())
    return true;

  unsigned addr_cells, size_cells;
  if (!parent.get_addr_size_cells(addr_cells, size_cells))
    return false;

  Node parent_parent = parent.parent_node();
  unsigned parent_addr_cells, parent_size_cells;
  if (!parent_parent.get_addr_size_cells(parent_addr_cells, parent_size_cells))
    return false;

  auto ranges = parent.get_prop_array("ranges", { addr_cells, parent_addr_cells,
                                                  size_cells });

  // If no ranges property is present, there exists no mapping between parent
  // and child address space.
  if (!ranges.is_present())
    return false; // no translation possible

  if (!ranges.is_valid())
    {
      parent.warn("Unexpected ranges property size.\n");
      return false;
    }

  // Empty ranges property?
  if (ranges.elements() == 0)
    return true; // identity mapping

  // Iterate over all the address space mappings
  for (unsigned i = 0; i < ranges.elements(); i++)
    {
      Range range{ranges.get(i, 0), ranges.get(i, 1), ranges.get(i, 2)};
      if (range.translate(addr, size))
        return translate_reg(parent_parent, addr, size);
    }
  return false;
}

bool Dt::Node::get_reg_array(Node parent, Reg_array_prop &regs) const
{
  if (!parent.is_valid())
    return false;

  unsigned addr_cells, size_cells;
  if (!parent.get_addr_size_cells(addr_cells, size_cells))
    return false;

  regs = get_prop_array("reg", { addr_cells, size_cells });
  return regs.is_valid();
}

bool Dt::Node::get_reg_val(Node parent, Reg_array_prop const &regs,
                           unsigned index, l4_uint64_t *out_addr,
                           l4_uint64_t *out_size) const
{
  if (index >= regs.elements())
    return false;

  l4_uint64_t addr = regs.get(index, 0);
  l4_uint64_t size = regs.get(index, 1);
  if (!translate_reg(parent, addr, size))
    return false;

  if (out_addr)
    *out_addr = addr;
  if (out_size)
    *out_size = size;
  return true;
}

Dt::Node Dt::get_clock(Node node, char const *name, int index) const
{
  Node result = Node(_fdt);

  if (name)
    index = node.stringlist_search("clock-names", name);

  if (index < 0)
    return result;

  node.for_each_phandle("clocks", "#clock-cells",
    [&index, &result](Node clock, Array) {
      if (index-- > 0)
        return Dt::Continue;

      result = clock;
      return Dt::Break;
    });

  return result;
}


void Dt::setup_memory() const
{
  // Iterate all memory nodes
  nodes_by_prop_value("device_type", "memory", 7, [](Dt::Node mem)
    {
      // One example for this is 'secram' with 'status = "disabled' and
      // secure-status = "okay".
      if (!mem.is_enabled())
        return;

      mem.for_each_reg([](l4_uint64_t start, l4_uint64_t sz)
        {
          // Ignore multiple similar given regions
          Region n = Region::start_size(start, sz, ".ram", Region::Ram);
          bool duplicate = false;
          for (Region *i = mem_manager->ram->begin();
               i != mem_manager->ram->end(); ++i)
            if (i->overlaps(n) && i->type() == n.type())
              {
                duplicate = true;
                break;
              }

          if (!duplicate)
            {
              info("Add memory to RAM: %lluMiB (%10llx - %10llx)\n",
                     sz >> 20, start, start + sz - 1);

              mem_manager->ram->add(n);
            }
        });
    });

  // Scan reserved memory regions
  Node rsrv_mem = node_by_path("/reserved-memory");
  if (rsrv_mem.is_valid())
    {
      info("Reserved memory areas:\n");
      rsrv_mem.for_each_subnode([](Dt::Node rsrv)
        {
          if (!rsrv.is_enabled())
            return;

          char const *name = rsrv.get_name(".reserved");
          rsrv.for_each_reg([=](l4_uint64_t start, l4_uint64_t sz)
            {
              info("  %20s: %10llx - %10llx\n", name, start, start + sz - 1);

              mem_manager->regions->add(
                Region::start_size(start, sz, name, Region::Arch));
            });
        });
    }

  // Add device tree to memory map
  mem_manager->regions->add(
    Region::start_size(_fdt, fdt_totalsize(_fdt), ".dtb", Region::Root));
}

l4_uint64_t Dt::cpu_release_addr() const
{
  Node cpus = node_by_path("/cpus");
  if (!cpus.is_valid())
    return -1ULL;

  l4_uint64_t cpu_release_addr = -1ULL;
  cpus.for_each_subnode([&](Dt::Node cpu)
    {
      if (!cpu.check_device_type("cpu"))
        return Dt::Continue; // Not a cpu node

      if (!cpu.stringlist_contains("enable-method", "spin-table"))
        // Assume all cores use the same enable method.
        return Dt::Break;

      l4_uint64_t crel_addr = -1ULL;
      cpu.get_prop_u64("cpu-release-addr", crel_addr);

      if (Verbose_dt)
        {
          l4_uint64_t mpid = -1ULL;
          cpu.get_reg(0, &mpid);
          info("CPU[%llx]: %llx\n", mpid, crel_addr);
        }

      if (cpu_release_addr == -1ULL)
        cpu_release_addr = crel_addr;
      else if (cpu_release_addr != crel_addr)
        cpu.warn("Sorry: Target uses core-specific CPU release addresses, but only one currently supported.\n");
      return Dt::Continue;
    });

  return cpu_release_addr;
}

Dt::Node Dt::get_stdout_uart(char const *compatible, Parse_irq_fn parse_irq,
                             L4_kernel_options::Uart *kuart,
                             unsigned int *kuart_flags) const
{
  Node chosen = node_by_path("/chosen");
  if (!chosen.is_valid())
    return Node();

  const char *stdout_path = chosen.get_prop_str("stdout-path");
  if (!stdout_path)
    return Node();

  const char *option_delim = strchrnul(stdout_path, ':');

  Node uart = node_by_path(stdout_path, option_delim - stdout_path);
  if (!uart.is_valid())
    return Node();

  if (compatible && !uart.check_compatible(compatible))
    return Node();

  // The optional uart options string has the following format:
  // <baud: number><parity: bool><bits: number><flow: bool>
  // For example, 115200n8. We are only interested in baud.
  if (unsigned long baud = strtoul(option_delim + 1, nullptr, 10))
    {
      kuart->baud = baud;
      *kuart_flags |= L4_kernel_options::F_uart_baud;
    }

  return parse_uart(uart, parse_irq, kuart, kuart_flags);
}

Dt::Node Dt::parse_uart(Node uart, Parse_irq_fn parse_irq,
                        L4_kernel_options::Uart *kuart,
                        unsigned int *kuart_flags) const
{
  if (!uart.is_valid() || !uart.is_enabled())
    return Node();

  l4_uint64_t mmio_addr;
  if (uart.get_reg(0, &mmio_addr))
    {
      kuart->access_type = L4_kernel_options::Uart_type_mmio;
      kuart->base_address = mmio_addr;
      *kuart_flags |= L4_kernel_options::F_uart_base;
    }
  else
    // Failed to get mmio address of UART.
    return Node();

  int irq = parse_irq(uart);
  if (irq >= 0)
    {
      kuart->irqno = irq;
      *kuart_flags |= L4_kernel_options::F_uart_irq;
    }

  l4_uint32_t base_baud;
  if (uart.get_prop_u32("clock-frequency", base_baud))
    {
      kuart->base_baud = base_baud;
    }
  else
    {
      // Otherwise, try to get frequency from first clock connected to the UART.
      Node clock = get_clock(uart, nullptr, 0);
      if (clock.is_valid() && clock.get_prop_u32("clock-frequency", base_baud))
        kuart->base_baud = base_baud;
    }

  l4_uint32_t baud;
  if (uart.get_prop_u32("current-speed", baud))
    {
      kuart->baud = baud;
      *kuart_flags |= L4_kernel_options::F_uart_baud;
    }

  l4_uint32_t reg_shift;
  if (uart.get_prop_u32("reg-shift", reg_shift))
    kuart->reg_shift = reg_shift;

  return uart;
}

// grep "d0 0d fe ed " minicom.log | perl -e 'print pack("C*", map { hex($_) } split / +/, <>)' > dtb
void Dt::dump() const
{
  auto size = fdt_totalsize(_fdt);
  printf("DT: Dumping device tree:\n");
  for (auto i = 0u; i < size; i++)
    printf("%02x ", static_cast<l4_uint8_t const *>(_fdt)[i]);
  printf("\n");
}
