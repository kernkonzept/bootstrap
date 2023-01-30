/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <endian.h>

#include <l4/util/mb_info.h>
#include <l4/sys/types.h>

#include "qemu_fw_cfg.h"
#include "qemu_ramfb.h"
#include "panic.h"
#include "support.h"

namespace
{

struct __attribute__((packed)) Ramfb_cfg
{
  l4_uint64_t addr;
  l4_uint32_t fourcc;
  l4_uint32_t flags;
  l4_uint32_t width;
  l4_uint32_t height;
  l4_uint32_t stride;
};
static_assert(sizeof(Ramfb_cfg) == 28, "Unexpected Ramfb cfg size.");

enum
{
  Ramfb_format = 0x34325258, // DRM_FORMAT_XRGB8888
  Ramfb_stride = 4,

  Ramfb_default_width  = 1280,
  Ramfb_default_height = 720,
};

void detect_ramfb_size(l4_uint32_t *width, l4_uint32_t *height)
{
  *width = Ramfb_default_width;
  *height = Ramfb_default_height;

  if (l4_uint32_t fbe_res_len = Fw_cfg::select_file("opt/org.l4re/fb_res"))
    {
      char fbe_res[32];
      Fw_cfg::read_str(fbe_res_len, fbe_res);

      char *sep = strchr(fbe_res, 'x');
      if (!sep)
        return;

      *width = strtoul(fbe_res, &sep, 10);
      *height = strtoul(sep + 1, nullptr, 10);
    }
}

}

bool setup_ramfb(l4util_l4mod_info *mbi)
{
  // Ramfb setup requires QEMU fw_cfg.
  if (!Fw_cfg::is_present())
    return false;

  l4_uint16_t ramfb_file;
  l4_uint32_t ramfb_file_size;
  if (!Fw_cfg::find_file("etc/ramfb", &ramfb_file, &ramfb_file_size))
    // Ramfb device not present.
    return false;

  if (ramfb_file_size != sizeof(Ramfb_cfg))
    {
      // Invalid config size.
      printf("  ramfb: Invalid config size.\n");
      return false;
    }

  l4_uint32_t width, height;
  detect_ramfb_size(&width, &height);
  printf("  ramfb: Resolution configured to %ux%u.\n", width, height);

  // Must be super-page aligned.
  l4_uint32_t fb_size = l4_round_size(width * height * Ramfb_stride,
                                      L4_SUPERPAGESHIFT);

  // Allocate memory for frame buffer.
  l4_addr_t fb = mem_manager->find_free_ram_rev(fb_size, 0, ~0UL,
                                                L4_SUPERPAGESHIFT);
  if (!fb)
    panic("fatal: could not allocate memory for frame buffer.\n");

  // mark the region as reserved
  mem_manager->regions->add(
    Region::start_size(fb, fb_size, ".fb", Region::Arch));

  // Clear frame buffer (black).
  memset(reinterpret_cast<void *>(fb), 0, fb_size);

  // Configure ramfb with allocated frame buffer.
  Ramfb_cfg cfg;
  cfg.addr = htobe64(fb);
  cfg.fourcc = htobe32(Ramfb_format);
  cfg.flags = htobe32(0);
  cfg.width = htobe32(width);
  cfg.height = htobe32(height);
  cfg.stride = htobe32(width * Ramfb_stride);
  Fw_cfg::select(ramfb_file);
  Fw_cfg::write(cfg);

  // Allocate and reserve region to hold VBE info for ramfb.
  unsigned vbe_size = sizeof(l4util_mb_vbe_ctrl_t) + sizeof(l4util_mb_vbe_mode_t);
  l4_addr_t vbe_addr = mem_manager->find_free_ram(vbe_size);
  if (!vbe_addr)
    {
      printf("  ramfb: Unable to reserve region to hold VBE config.\n");
      return false;
    }
  mem_manager->regions->add(Region::start_size(vbe_addr, vbe_size, ".vbe",
                                               Region::Root, L4_FPAGE_RW));

  // Add ramfb VBE info to MBI.
  mbi->vbe_ctrl_info = vbe_addr;
  mbi->vbe_mode_info = mbi->vbe_ctrl_info + sizeof(l4util_mb_vbe_ctrl_t);

  auto *vci = reinterpret_cast<l4util_mb_vbe_ctrl_t *>(mbi->vbe_ctrl_info);
  auto *vmi = reinterpret_cast<l4util_mb_vbe_mode_t *>(mbi->vbe_mode_info);

  // Use the structures phys_base plus the following reserved1 field to
  // hold a 64-bit value
  vmi->phys_base    = fb;
  vmi->reserved1    = static_cast<l4_uint64_t>(fb) >> 32;
  vmi->x_resolution = width;
  vmi->y_resolution = height;

  vci->total_memory = fb_size >> 16;; // in 64k chunks

  vmi->red_mask_size = 8;
  vmi->green_mask_size = 8;
  vmi->blue_mask_size = 8;

  vmi->red_field_position = 16;
  vmi->green_field_position = 8;
  vmi->blue_field_position = 0;

  vmi->bytes_per_scanline = width * Ramfb_stride;
  vmi->bits_per_pixel = Ramfb_stride * 8;

  mbi->flags |= L4UTIL_MB_VIDEO_INFO;

  return true;
}
