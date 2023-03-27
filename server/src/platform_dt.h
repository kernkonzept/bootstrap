/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorznynski <adam@l4re.org>
 */

#include <platform.h>
#include <dt.h>

class Platform_dt : public Platform_base,
                    public Boot_modules_image_mode
{
public:
  Boot_modules *modules() override { return this; }
  void setup_memory_map() override;
  virtual void post_memory_hook() {}
  bool have_a_dt() override { return true; }

  Dt dt;
};
