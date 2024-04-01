/*
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <mod_info.h>

short Mod_info::index() const
{ return this - mod_header->mods().begin(); }
