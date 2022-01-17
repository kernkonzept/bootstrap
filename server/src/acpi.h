/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include "support.h"

namespace Acpi {

bool check_signature(char const *sig, char const *reference);

// Generic address structure
struct Gas
{
  enum { Mmio, Io, Pci_cfg, Ec, Smbus, Cmos, Pci_bar, Ipmi, Gpio, Serial, Pcc };
  l4_uint8_t asid;
  l4_uint8_t width;
  l4_uint8_t offset;
  enum { Undefined, Byte, Word, Dword, Qword };
  l4_uint8_t size;
  l4_uint64_t address;
} __attribute__((packed));

struct Table_head
{
  char       signature[4];
  l4_uint32_t len;
  l4_uint8_t  rev;
  l4_uint8_t  chk_sum;
  char       oem_id[6];
  char       oem_tid[8];
  l4_uint32_t oem_rev;
  l4_uint32_t creator_id;
  l4_uint32_t creator_rev;

  bool checksum_ok() const;
  void print_info() const;
} __attribute__((packed));


class Sdt
{
  template< typename T >
  struct Sdt_p : public Table_head
  {
    T ptrs[0];
  } __attribute__((packed));

  typedef Sdt_p<l4_uint32_t> Rsdt;
  typedef Sdt_p<l4_uint64_t> Xsdt;

public:
  bool init(void *root);

  template<typename T>
  T find(char const *sig) const
  {
    return reinterpret_cast<T>(find_head(sig));
  }

private:
  Table_head const *find_head(char const *sig) const;
  unsigned entries() const;
  Table_head const *entry(unsigned i) const;
  void print_summary() const;

  Xsdt const *_xsdt = 0;
  Rsdt const *_rsdt = 0;
};

struct Fadt : public Table_head
{
    l4_uint32_t                  facs;
    l4_uint32_t                  dsdt;
    l4_uint8_t                   model;
    l4_uint8_t                   preferred_profile;
    l4_uint16_t                  sci_interrupt;
    l4_uint32_t                  smi_command;
    l4_uint8_t                   acpi_enable;
    l4_uint8_t                   acpi_disable;
    l4_uint8_t                   s4_bios_request;
    l4_uint8_t                   pstate_control;
    l4_uint32_t                  pm1a_event_block;
    l4_uint32_t                  pm1b_event_block;
    l4_uint32_t                  pm1a_control_block;
    l4_uint32_t                  pm1b_control_block;
    l4_uint32_t                  pm2_control_block;
    l4_uint32_t                  pm_timer_block;
    l4_uint32_t                  gpe0_block;
    l4_uint32_t                  gpe1_block;
    l4_uint8_t                   pm1_event_length;
    l4_uint8_t                   pm1_control_length;
    l4_uint8_t                   pm2_control_length;
    l4_uint8_t                   pm_timer_length;
    l4_uint8_t                   gpe0_block_length;
    l4_uint8_t                   gpe1_block_length;
    l4_uint8_t                   gpe1_base;
    l4_uint8_t                   cst_control;
    l4_uint16_t                  c2_latency;
    l4_uint16_t                  c3_latency;
    l4_uint16_t                  flush_size;
    l4_uint16_t                  flush_stride;
    l4_uint8_t                   duty_offset;
    l4_uint8_t                   duty_width;
    l4_uint8_t                   day_alarm;
    l4_uint8_t                   month_alarm;
    l4_uint8_t                   century;
    l4_uint16_t                  iapc_boot_flags;
    l4_uint8_t                   reserved;
    enum { Reset_reg_sup = 1U << 10, Hw_reduced_acpi = 1UL << 20 };
    l4_uint32_t                  flags;
    Gas                          reset_register;
    l4_uint8_t                   reset_value;
    enum { Psci_compliant = 1, Psci_use_hvc = 2 };
    l4_uint16_t                  arm_boot_flags;
    l4_uint8_t                   minor_revision;
    l4_uint64_t                  x_facs;
    l4_uint64_t                  x_dsdt;
    Gas                          x_pm1a_event_block;
    Gas                          x_pm1b_event_block;
    Gas                          x_pm1a_control_block;
    Gas                          x_pm1b_control_block;
    Gas                          x_pm2_control_block;
    Gas                          x_pm_timer_block;
    Gas                          x_gpe0_block;
    Gas                          x_gpe1_block;
    Gas                          sleep_control;
    Gas                          sleep_status;
    l4_uint64_t                  hypervisor_id;
} __attribute__((packed));

struct Spcr : public Table_head
{
  enum
  {
    Nsc16550 = 0, Compat16550, Max311xe, Arm_pl011, Msm8x60, Nvidia16550,
    Ti_omap,
    APM88xxxx = 8, Msm8974, Sam5250, Intel_usif, Imx6, Arm_sbsa_32bit,
    Arm_sbsa, Arm_dcc, Bcm2835, Sdm845_18432, Gas16550, Sdm845_7372, Intel_lpss
  };
  l4_uint8_t type;
  l4_uint8_t reserved[3];
  Gas base;
  enum { Pic = 1, Apic = 2, Sapic = 4, Gic = 8 };
  l4_uint8_t irq_types;
  l4_uint8_t irq;
  l4_uint32_t irq_gsiv;
  enum { As_is, B9600 = 3, B19200 = 4, B57600 = 6, B115200 = 7 };
  l4_uint8_t baud_rate;
  l4_uint8_t parity;
  l4_uint8_t stop_bits;
  enum { Dcd = 1, Rts_cts = 2, Xon_xoff = 4 };
  l4_uint8_t flow_control;
  enum { Vt100, Vt100_plus, Vt_utf8, Ansi };
  l4_uint8_t terminal_type;
  l4_uint8_t language;
  l4_uint16_t pci_device_id;
  l4_uint16_t pci_vendor_id;
  l4_uint8_t pci_bus_num;
  l4_uint8_t pci_device_num;
  l4_uint8_t pci_function_num;
  l4_uint32_t pci_flags;
  l4_uint8_t pci_segment;
  l4_uint32_t uart_clock_freq;
} __attribute__((packed));

}
