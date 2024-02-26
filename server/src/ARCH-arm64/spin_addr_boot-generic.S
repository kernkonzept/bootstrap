/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *	      Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *	      Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "ARCH-arm64/spin_addr_boot.h"

.section .text.init,"ax"

.global cpu_bootup_code_start
cpu_bootup_code_start:

.global platform_cpu_bootup
.type platform_cpu_bootup, #function
platform_cpu_bootup:
	mrs	x9, MPIDR_EL1              /* CPU ID */
	and	x9, x9, #~(0xff << 24)     /* Just keep Aff[0-3] fields */
	cmp	x9, #0                     /* CPU0 continues with bootstrap */
	beq	do_bootstrap

	/* CPU1+ wait for bootup */
	spin_addr_boot_wait

.global cpu_bootup_code_end
cpu_bootup_code_end:
