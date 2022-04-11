/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include "arch/arm/mpu_consts.h"

/*
 * The CR52 boot is a bit more involved. We are starting from address 0x0 which
 * is an executable remapping of the RT-SRAM located at address 0xeb200000.
 * Only the first 128K are remapped so we have to jump to our link addess.
 * Unfortunately everything above 0x80000000 are not execuable on the
 * architectually defined MPU background region. This implies that we have to
 * setup the MPU first.
 *
 * Region 0 must cover the RAM where Fiasco is later executed. This is required
 * by the Fiasco bootstrap code because the MPU must stay enabled all the time
 * and the region covering the boot code must not vanish.
 */
.macro crt0_platform_early_init
	/* Read our real boot address from APMU.CR52BARP */
	ldr	r4, =0xE617033C
	ldr	r6, [r4]

	/* Disable MPU */
	mrc	p15, 4, r4, c1, c0, 0	/* HSCTLR */
	bic	r4, r4, #1		/* disable MPU */
	mcr	p15, 4, r4, c1, c0, 0	/* HSCTLR */
	isb

	/* Set memory attribute indirection registers */
	ldr	r4, =MAIR0_BITS
	ldr	r5, =MAIR1_BITS
	mcr	p15, 4, r4, c10, c2, 0	/* HMAIR0 */
	mcr	p15, 4, r5, c10, c2, 1	/* HMAIR1 */

	/* Region 0: whole RAM as normal non-cached, executable */
	ldr	r4, =0xeb200000		/* NSH, RW@EL2, executable */
	ldr	r5, =(0xeb3fffc0 | MPU_ATTR_NORMAL | 1)
	mcr	p15, 4, r4, c6, c8, 0	/* HPRBAR0 */
	mcr	p15, 4, r5, c6, c8, 1	/* HPRLAR0 */

	/* Region 1: SCIF0 */
	ldr	r4, =0xe6e60001		/* NSH, RW@EL2, XN */
	ldr	r5, =(0xe6e67fc0 | MPU_ATTR_DEVICE | 1)
	mcr	p15, 4, r4, c6, c8, 4	/* HPRBAR1 */
	mcr	p15, 4, r5, c6, c8, 5	/* HPRLAR1 */

	/* Region 2: RT-SRAM mirror at 0x00000000 (current PC is there) */
	ldr	r4, =0x00000000		/* NSH, RW@EL2, executable */
	ldr	r5, =(0x0003ffc0 | MPU_ATTR_NORMAL | 1)
	mcr	p15, 4, r4, c6, c9, 0	/* HPRBAR2 */
	mcr	p15, 4, r5, c6, c9, 1	/* HPRLAR2 */

	/* Enable just regions 0, 1 and 2 */
	mov	r4, #7
	mcr	p15, 4, r4, c6, c1, 1	/* HPRENR */

	isb

	/* Enable MPU */
	mrc	p15, 4, r4, c1, c0, 0	/* HSCTLR */
	orr	r4, r4, #1		/* enable MPU */
	mcr	p15, 4, r4, c1, c0, 0	/* HSCTLR */

	isb

	/*
	 * Do an absolute jump without creating a relocation in the text
	 * segment. This would break the PIE build.
	 */
	bic	r4, r6, #0xff
	adr	r5, 1f
	orr	r4, r4, r5
	bx	r4
1:
.endm
