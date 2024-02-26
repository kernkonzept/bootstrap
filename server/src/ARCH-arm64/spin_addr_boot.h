/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *	      Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *	      Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

.macro spin_addr_boot_wait
1:	ldr	x9, mp_launch_spin_addr
	cmp	x9, #0
	beq	2f                         /* Not yet */
	blr	x9
2:	wfe
	b	1b

	.align 3
	.global mp_launch_spin_addr
mp_launch_spin_addr:
	.space 8
.endm

