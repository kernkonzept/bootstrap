/*
 * Copyright (C) 2021, 2023-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
/**
 * \file
 * \brief  Support for ARM's AEM FVP Base platform
 */

#include "arm_fvp_base.h"

namespace {

class Platform_arm_fvp_base : public Platform_arm_fvp_base_common<false>
{};

}

REGISTER_PLATFORM(Platform_arm_fvp_base);
