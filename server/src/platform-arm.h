/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Copyright (C) 2015,2017,2019-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "platform.h"
#include "koptions-def.h"
#include "support.h"
#include "arch/arm/mem.h"

#include <l4/sys/kip.h>

class Platform_arm : public Platform_base
{
public:
  enum class EL_Support { EL2, EL1, Unknown };
  enum Psci_method { Psci_unsupported, Psci_smc, Psci_hvc };

  void init_regions() override
  {
    // Ensure later stages do not overwrite the CPU boot-up code
    extern char cpu_bootup_code_start[] __attribute__((weak));
    extern char cpu_bootup_code_end[] __attribute__((weak));
    if (cpu_bootup_code_start)
      {
        l4_size_t const size = cpu_bootup_code_end - cpu_bootup_code_start;
        mem_manager->regions->add(Region::start_size(cpu_bootup_code_start, size,
                                                     ".cpu_boot", Region::Root),
                                  true);
      }

    modules()->init_mod_regions();
  }

  void set_psci_method(Psci_method method) { _psci_method = method; }
  Psci_method get_psci_method() const { return _psci_method; }

  void reboot_psci()
  {
    register unsigned long r0 asm("r0") = 0x84000009;
#ifdef ARCH_arm
    asm volatile(".arch armv7-a\n"
                 ".arch_extension sec\n"
                 ".arch_extension virt\n");
#endif
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

#ifdef ARCH_arm
  // Must be executed in ARM mode because it uses inline assembly with ARM
  // instruction.
  __attribute__((target("arm")))
#endif
  void setup_kernel_config(l4_kernel_info_t *kip) override;

  void setup_kernel_options(L4_kernel_options::Options *lko) override
  {
    // If we do not get an spin address from DT, all cores might start
    // at the same time and are caught by bootstrap
    extern l4_umword_t mp_launch_spin_addr __attribute__((weak));
    asm volatile("" : : : "memory");
    Barrier::dmb_cores();
    lko->core_spin_addr = (l4_uint64_t)&mp_launch_spin_addr;
  }

  void set_uart_compatible(L4_kernel_options::Uart *kuart, const char *compatible)
  {
    strncpy(kuart->compatible_id, compatible, sizeof(kuart->compatible_id) - 1);
    kuart->compatible_id[sizeof(kuart->compatible_id) - 1] = '\0';
  }

  void setup_kernel_config_arm_common(l4_kernel_info_t *kip);

private:
  EL_Support kernel_type = EL_Support::Unknown;
  Psci_method _psci_method = Psci_smc;

  virtual bool arm_switch_to_hyp() { return false; }
};



