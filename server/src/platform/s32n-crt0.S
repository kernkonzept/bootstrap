/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#include "ARCH-arm/spin_addr_boot.h"

.section .text.init,"ax"

.global cpu_bootup_code_start
cpu_bootup_code_start:

// First CPU in each cluster is the boot AMP node. Other CPUs have to wait...
.global platform_cpu_bootup
.type platform_cpu_bootup, #function
platform_cpu_bootup:
	mrc	p15, 0, r4, c0, c0, 5      /* CPU ID */
	tst	r4, #0xff                  /* Test Aff0 field */
#if !defined(CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_DISABLED)
	tsteq	r4, #(0xff <<  8)      /* Test Aff1 field */
#endif /* !CONFIG_BOOTSTRAP_PF_S32N_RTU_CL0_DISABLED */
	beq	do_bootstrap               /* Boot CPU continues with bootstrap */
	spin_addr_boot_wait            /* Secondary CPUs wait for bootup */

.global cpu_bootup_code_end
cpu_bootup_code_end:
