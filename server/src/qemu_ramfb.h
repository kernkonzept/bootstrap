/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

#include <l4/util/l4mod.h>

/**
 * Configure QEMU ramfb and fill in the VESA information in the MBI accordingly.
 *
 * \param mbi  MBI structure to be populated with VESA information.
 *
 * \return Whether ramfb initialization was successful.
 *
 * \pre QEMU firmware configuration interface (Fw_cfg) must have been initialized.
 *
 * \note The ramfb device can be enabled in QEMU via the `-device ramfb` option.
 * \note The resolution of the ramfb can be configured via the L4Re-specific
 *       `opt/org.l4re/fb_res` fw_cfg item, which can be insterted via the QEMU
 *       command line: `-fw_cfg opt/org.l4re/fb_res,string=1920x1080`
 */
bool setup_ramfb(l4util_l4mod_info *mbi);
