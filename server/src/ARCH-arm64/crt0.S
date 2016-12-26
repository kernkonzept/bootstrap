/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/* -*- c -*- */

.section .text.init,#alloc,#execinstr
.type _start,#function
.globl _start
_start:
	/*
	 * It might be the case that we're not run at the position where we
	 * have been linked to. If this is the case we copy ourselves to the
	 * position we're linked to.
	 */
	adr	x4, run		/* Running version */
	ldr	x5, .LCrun	/* supposed to be version */
	cmp	x4, x5		/* If equal ... */
	b.eq	run		/* ... go to run */

	/* Disable caches as we're moving code around */
	ic iallu
	mrs x0, SCTLR_EL1
	bic x0, x0, #0x0004
	bic x0, x0, #0x1000
	msr SCTLR_EL1, x0

	/* Figure how to move */
        ldr     x7, .LCend_bin
	subs	x8, x5, x4	/* x8 is the distance between the blocks */
	b.pl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	x9, x5			/* r9: run address */
	ldr	x0, .LCstart_bin
	ldr	x3, 3f
	str	x3, [x0], #8
	ldr	x3, 33f
	str	x3, [x0], #8
	ldr	x3, 35f
	str	x3, [x0], #8
	ldr	x10, .LCstart_bin
	br	x10

3:	ldr	x6, [x4], #8
	str	x6, [x5], #8
33:	cmp	x5, x7
	b.lt	3b
35:	br	x9
	nop

	/* Copy behind, copy backwards */
move_behind:
	sub	x8, x7, x8	/* r8 points to the end of source image */
3:	ldr	x6, [x8, #-8]!	/* Take bytes */
	str	x6, [x7, #-8]!	/* Put bytes */
	cmp	x5, x7
	b.lt	3b
	ldr	x10, .LCrun
	br	x10


.LCrun:       .8byte run
.LCstart_bin: .8byte _start
.LCend_bin:   .8byte _module_data_end

run:
do_bootstrap:
	ldr	x3, .LCcrt0_tramppage      /* Load address of tramppage var */
	str	x9, [x3]                   /* Store SP in variable          */
	adr	x9, crt0_stack_high
	mov	sp, x9

	mrs	x8, cpacr_el1
	orr	x8, x8, #0x300000 // fpen
	msr	cpacr_el1, x8


	mov	x0, x2                     /* ATAG pointer */
	bl	__main
1:	b	1b

.LCcrt0_tramppage:      .quad crt0_tramppage

.section ".bss"

	.global	crt0_tramppage
crt0_tramppage:
	.space 8

	.global crt0_stack_low
	.align 4
crt0_stack_low:
	.space	8192
	.global crt0_stack_high
crt0_stack_high:
