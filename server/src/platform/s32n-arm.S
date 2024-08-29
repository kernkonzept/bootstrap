/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ARCH-arm/spin_addr_boot.h"

.section .text.init,"ax"

.global cpu_bootup_code_start
cpu_bootup_code_start:

.global platform_cpu_bootup
.type platform_cpu_bootup, #function
platform_cpu_bootup:
	mrc	p15, 0, r4, c0, c0, 5      /* CPU ID */
	tst	r4, #0xff                  /* Test Aff0 field */
#if !defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_DISABLED)
	tsteq	r4, #(0xff <<  8)          /* Test Aff1 field */
#endif /* !CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_DISABLED */
	beq	do_bootstrap               /* CPU0 continues with bootstrap */

	/* CPU1+ wait for bootup */
	spin_addr_boot_wait

.global cpu_bootup_code_end
cpu_bootup_code_end:
