/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *	      Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *	      Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

.section .text.init,"ax"

.global cpu_bootup_code_start
cpu_bootup_code_start:

.global platform_cpu_bootup
.type platform_cpu_bootup, #function
platform_cpu_bootup:
	mrc	p15, 0, r4, c0, c0, 5      /* CPU ID */
	tst	r4, #0x0f                  /* Test Aff0 field */
	beq	do_bootstrap               /* CPU0 continues with bootstrap */

/* CPU1+ wait for bootup */

	// IRQs, FIQs off
	mrs r4, cpsr
	orr r4, #0xc0
	msr cpsr_c, r4

	// enable GIC CPU interface + prio mask for IRQs
	mrc	p15, 0, r8, c0, c0, 0    // r8: cpu-id
	and	r8, r8, #0x70
	cmp	r8, #0x70

	// get board ID and retrieve MPCore-base from table
	movne	r7, #0x10000000
	moveq	r7, #0x00010000
	orreq	r7, #0x1c000000
	ldr	r5, [r7]
	bic	r5, #0xff

	adr	r6, .Lboard_data_table
2:
	ldr	r4, [r6]
	cmp	r4, #0
	beq	3f
	cmp	r4, r5
	bne	4f

	ldr	r9, [r6, #4]
	cmp	r8, r9
	beq	3f
	cmp	r9, #0
	beq	3f
4:	add	r6, #12
	b	2b

3:
	ldr	r4, [r6, #8]
	mov	r8, #0x1
	str	r8, [r4, #0]
	mov	r8, #0xf0
	str	r8, [r4, #4]

1:
	ldr	r6, [r7, #0x30]
	cmp	r6, #0
	movne	pc, r6
	.inst	0xe320f003 /* wfi */
	ldr	r8, [r4, #12]
	str	r8, [r4, #16]
	b	1b

.Lboard_data_table:
	/* VExpress */
	.word	0x1190f500 /* Board ID */
	.word	0x00000070 /* CPU ID */
	.word	0x2c002000 /* MPCore base */

	.word	0x1190f500 /* Board ID */
	.word	0x00000000 /* CPU ID */
	.word	0x1e000100 /* MPCore base */

	/* Default value (with #0) must come last! */
	/* Realview */
	.word	0
	.word	0
	.word	0x1f000100

.global cpu_bootup_code_end
cpu_bootup_code_end:
