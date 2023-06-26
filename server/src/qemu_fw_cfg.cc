/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <endian.h>
#include <string.h>
#include <stdio.h>

#include <l4/cxx/utils>
#include <l4/sys/compiler.h>

#include "panic.h"
#include "qemu_fw_cfg.h"

namespace
{

/// Enforces ordering between MMIO register access and/or shared memory accesses.
#if defined(__ARM_ARCH) && (__ARM_ARCH == 5 || __ARM_ARCH == 6)
static inline void mb() { asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }
#elif defined(__ARM_ARCH) && __ARM_ARCH == 7
static inline void mb() { asm volatile ("dsb" : : : "memory"); }
#elif defined(__ARM_ARCH) && __ARM_ARCH >= 8
// Since ARMv8 the memory model is other-multi-copy atomic, i.e. a memory write
// observed by an observer is also visible to all other observers. In other
// words, the memory write is either visible only locally to the originator (for
// example, in its store buffer) or visible globally to all observers, i.e. it
// is propagated to all observers at the same time.
// Therefore, an dmb is sufficient to ensure that not only the CPU and device
// agree on the order of MMIO register accesses and shared memory accesses, but
// also a potential DMA master via which the device performs DMA operations.
static inline void mb() { asm volatile ("dmb osh" : : : "memory"); }
#elif defined(__mips__)
static inline void mb() { asm volatile ("sync" : : : "memory"); }
#elif defined(__amd64__) || defined(__i386__) || defined(__i686__)
static inline void mb() { asm volatile ("mfence" : : : "memory"); }
#elif defined(__riscv)
static inline void mb() { asm volatile ("fence iorw, iorw" : : : "memory"); }
#else
#error Missing proper memory write barrier
#endif

enum Fw_cfg_item_slectors
{
  // Item selectors defined by Qemu
  Fw_cfg_signature     = 0x00,
  Fw_cfg_if_version    = 0x01,
  Fw_cfg_uuid          = 0x02,
  Fw_cfg_file_dir      = 0x19,

  // dynamically added entries found through Fw_cfg_file_dir start here
  Fw_cfg_dynamic_start = 0x20,
};
enum
{
  Fw_cfg_version_traditional   = 1,
  Fw_cfg_version_dma_supported = 2,

  Fw_cfg_reg_selector_io       = 0x00,
  Fw_cfg_reg_data_io           = 0x01,
  Fw_cfg_reg_dma_addr_io       = 0x04,

  Fw_cfg_dma_control_error     = 0x01,
  Fw_cfg_dma_control_read      = 0x02,
  Fw_cfg_dma_control_skip      = 0x04,
  Fw_cfg_dma_control_select    = 0x08,
  Fw_cfg_dma_control_write     = 0x10,
};

enum
{
  Qemu_fw_cfg_signature = (('Q' << 24) | ('E' << 16) | ('M' << 8) | 'U'),
};

struct Fw_cfg_dma_access
{
  // everything is given in big endian!
  l4_uint32_t control;
  l4_uint32_t length;
  l4_uint64_t address;
};
static_assert(sizeof(Fw_cfg_dma_access) == 16, "Unexpected DMA access size.");

struct Fw_cfg_file
{
  l4_uint32_t size;    // size of this fw_cfg item, big endian
  l4_uint16_t select;  // selector key for fw_cfg item, big endian
  l4_uint16_t reserved;
  char name[56]; // NUL-terminated ascii
};
static_assert(sizeof(Fw_cfg_file) == 64, "Unexpected dir entry size.");

struct Fw_cfg_dir
{
  l4_uint32_t count; // number of entries, big endian
  Fw_cfg_file entries[];
};
static_assert(sizeof(Fw_cfg_dir) == 4, "Unexpected dir size.");

bool _fw_cfg_present = false;

}

bool
Fw_cfg::init()
{
  // Assume fw_cfg is present, otherwise the following select and read would fail.
  _fw_cfg_present = true;

  select(Fw_cfg_signature);
  l4_uint32_t signature = be32toh(read<l4_uint32_t>());
  if (signature != Qemu_fw_cfg_signature)
    {
      _fw_cfg_present = false;
      return false;
    }

  select(Fw_cfg_if_version);
  l4_uint32_t features = le32toh(read<l4_uint32_t>());
  if (!(features & Fw_cfg_version_dma_supported))
    panic("fw_cfg: Does not support DMA interface.");

  return true;
}

bool
Fw_cfg::is_present()
{
  return _fw_cfg_present;
}

bool
Fw_cfg::find_file(char const *name, l4_uint16_t *selector, l4_uint32_t *size)
{
  select(Fw_cfg_file_dir);

  Fw_cfg_dir dir = read<Fw_cfg_dir>();
  for(l4_uint32_t i = 0; i < be32toh(dir.count); i++)
    {
      Fw_cfg_file file = read<Fw_cfg_file>();
      if (!strncmp(file.name, name, sizeof(file.name)))
        {
          if (selector) *selector = be16toh(file.select);
          if (size) *size = be32toh(file.size);
          return true;
        }
    }

  return false;
}

l4_uint32_t
Fw_cfg::select_file(char const *name)
{
  l4_uint16_t sel;
  l4_uint32_t size;
  if (!find_file(name, &sel, &size))
    return 0;

  select(sel);
  return size;
}

void
Fw_cfg::read_bytes(l4_uint32_t size, l4_uint8_t *buffer)
{
  if (!is_present())
    panic("fw_cfg: Not present.");

  dma_transfer(Fw_cfg_dma_control_read, size, buffer);
}

void
Fw_cfg::write_bytes(l4_uint32_t size, l4_uint8_t const *buffer)
{
  if (!is_present())
    panic("fw_cfg: Not present.");

  dma_transfer(Fw_cfg_dma_control_write, size, const_cast<l4_uint8_t *>(buffer));
}

void
Fw_cfg::skip_bytes(l4_uint32_t size)
{
  if (!is_present())
    panic("fw_cfg: Not present.");

  dma_transfer(Fw_cfg_dma_control_skip, size, nullptr);
}

void
Fw_cfg::dma_transfer(l4_uint32_t control, l4_uint32_t size, l4_uint8_t *buffer)
{
  // Ensure control is valid.
  if (   control != Fw_cfg_dma_control_read
      && control != Fw_cfg_dma_control_write
      && control != Fw_cfg_dma_control_skip)
    panic("fw_cfg: Unsupported DMA operation: %u", control);

  // Nothing to do.
  if (size == 0)
    return;

  Fw_cfg_dma_access access;
  access.control = htobe32(control);
  access.length = htobe32(size);
  access.address = htobe64(reinterpret_cast<l4_addr_t>(buffer));

  // The data in the buffer and the access descriptor must be visible before
  // starting the DMA transfer.
  mb();

  trigger_dma(reinterpret_cast<l4_addr_t>(&access));

  // Wait for transfer to complete. On successful completion all bits are
  // cleared.
  while (l4_uint32_t status = cxx::access_once(&access.control))
    {
      if (be32toh(status) & Fw_cfg_dma_control_error)
        panic("fw_cfg: DMA transfer failed.");
    }

  // Ensure the transferred data is visible.
  mb();
}
