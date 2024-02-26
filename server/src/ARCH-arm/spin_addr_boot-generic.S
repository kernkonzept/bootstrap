/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *	      Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *	      Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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
	tsteq	r4, #(0xff <<  8)          /* Test Aff1 field */
	tsteq	r4, #(0xff << 16)          /* Test Aff2 field */
	beq	do_bootstrap               /* CPU0 continues with bootstrap */

	/* CPU1+ wait for bootup */
	spin_addr_boot_wait

.global cpu_bootup_code_end
cpu_bootup_code_end:
