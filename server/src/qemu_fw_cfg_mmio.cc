/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <endian.h>

#include <l4/cxx/type_traits>
#include <l4/drivers/asm_access.h>
#include <l4/sys/compiler.h>

#include "panic.h"
#include "qemu_fw_cfg.h"

namespace
{

enum
{
  Fw_cfg_reg_selector_mmio     = 0x08,
  Fw_cfg_reg_data_mmio         = 0x00,
  Fw_cfg_reg_dma_addr_mmio     = 0x10,
};

l4_addr_t _fw_cfg_mmio_base_addr = 0;

void
write_reg_selector(l4_addr_t reg_selector, l4_uint16_t selector)
{
  Asm_access::write(htobe16(selector),
                    reinterpret_cast<l4_uint16_t *>(reg_selector));
}

void
write_reg_dma_addr(l4_addr_t reg_dma_addr, l4_addr_t dma_desc)
{
  using Dma_addr = cxx::conditional_t<sizeof(l4_addr_t) == 4, l4_uint32_t,
                                                              l4_uint64_t>;
  static_assert(sizeof(l4_addr_t) == sizeof(Dma_addr),
                "Unsupported address size.");

  if (sizeof(Dma_addr) == 4)
    // Write to least signficant half trigers DMA operation.
    Asm_access::write(htobe32(dma_desc),
                      reinterpret_cast<Dma_addr *>(reg_dma_addr + 4));
  else
    Asm_access::write(htobe64(dma_desc),
                      reinterpret_cast<Dma_addr *>(reg_dma_addr));
}

}

bool
Fw_cfg::init_mmio(l4_addr_t base_addr)
{
  _fw_cfg_mmio_base_addr = base_addr;
  return init();
}

void
Fw_cfg::select(l4_uint16_t selector)
{
  if (!is_present())
    panic("fw_cfg: Not present.");

  write_reg_selector(_fw_cfg_mmio_base_addr + Fw_cfg_reg_selector_mmio, selector);
}

void Fw_cfg::trigger_dma(l4_addr_t dma_desc)
{
  write_reg_dma_addr(_fw_cfg_mmio_base_addr + Fw_cfg_reg_dma_addr_mmio, dma_desc);
}
