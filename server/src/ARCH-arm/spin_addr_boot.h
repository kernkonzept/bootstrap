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
	adr	r0, mp_launch_spin_addr

1:	ldr	r1, [r0]
	cmp	r1, #0
	bxne	r1
	.inst 0xe320f002  // wfe
	b       1b

	.global mp_launch_spin_addr
mp_launch_spin_addr:
	.space 4
.endm
