/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
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
