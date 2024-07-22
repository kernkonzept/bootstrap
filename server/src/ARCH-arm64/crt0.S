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

#include <l4/bid_config.h>

.macro set_sctlr sctlr_reg, tmp
	mrs	\tmp, \sctlr_reg
	bic	\tmp, \tmp, #0x0004
	bic	\tmp, \tmp, #0x1000
	msr	\sctlr_reg, \tmp
.endm

#define MIMIC_A_VMLINUZ

.section .text.init,"ax"
.type _start,#function
.globl _start
_start:
/* Some bootloaders like it this way, for others it won't harm */
#ifdef MIMIC_A_VMLINUZ
	nop
	b 10f
	.8byte 0x80000		/* text_offset */
	.8byte 0		/* image_size */
	.8byte 0		/* flags */
	.8byte 0, 0, 0		/* reserved */
	.ascii "ARM\x64"	/* magic */
	.4byte 0		/* reserved */
10:
#endif
#ifndef CONFIG_BID_PIE
	/*
	 * It might be the case that we're not run at the position where we
	 * have been linked to. If this is the case we copy ourselves to the
	 * position we're linked to. If L4Re is built with PIE support we
	 * can run from anywhere and there is no need to copy ourselves around.
	 */
	adr	x4, run		/* Running version */
	ldr	x5, .LCrun	/* supposed to be version */
	cmp	x4, x5		/* If equal ... */
	b.eq	run		/* ... go to run */

	/* Disable caches as we're moving code around */
	ic	iallu
	mrs	x12, CurrentEL
	ubfx	x12, x12, #2, #2
	cmp	x12, #3
	b.eq	.Lsctlr_el3
	cmp	x12, #2
	b.eq	.Lsctlr_el2

	set_sctlr SCTLR_EL1, x12
	b .Lsctlr_done
.Lsctlr_el2:
	set_sctlr SCTLR_EL2, x12
	b .Lsctlr_done
.Lsctlr_el3:
	set_sctlr SCTLR_EL3, x12
.Lsctlr_done:

	/* Figure how to move */
        ldr     x7, crt_end_bin
	add     x7, x7, #7	/* Round up to word size for ldr and str... */
	bic     x7, x7, #7	/* ...below */
	subs	x8, x5, x4	/* x8 is the distance between the blocks */
	b.pl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	x9, x5			/* x9: run address */
	ldr	x12, .LCstart_bin
	ldr	x13, 3f
	str	x13, [x12], #8
	ldr	x13, 33f
	str	x13, [x12], #8
	ldr	x13, 35f
	str	x13, [x12], #8
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
	sub	x8, x7, x8	/* x8 points to the end of source image */
3:	ldr	x6, [x8, #-8]!	/* Take bytes */
	str	x6, [x7, #-8]!	/* Put bytes */
	cmp	x5, x7
	b.lt	3b
	ldr	x10, .LCrun
	br	x10


	.align 3
.LCrun:       .8byte run
.LCstart_bin: .8byte _start
crt_end_bin:  .8byte _module_data_end

run:
#endif
	b	platform_cpu_bootup

.type platform_cpu_bootup, #function
.weak platform_cpu_bootup
platform_cpu_bootup:
	/* Can be overridden by platform for special MP boot */

.global do_bootstrap
.type do_bootstrap, #function
do_bootstrap:
	adrp	x9, crt0_stack_high
	add	x9, x9, :lo12:crt0_stack_high
	mov	sp, x9

	mrs	x8, cpacr_el1
	orr	x8, x8, #0x300000 // fpen
	msr	cpacr_el1, x8

#ifdef CONFIG_BID_PIE
	/* Calculate load address... subtract link address from real one */
	adrp    x0, :got:_start
	ldr     x0, [x0, #:got_lo12:_start]
	adr     x1, _start
	sub     x0, x1, x0

	/* Do relocations */
	bl reloc_static_pie
#endif

	bl	__main
1:	b	1b

.section ".bss"

	.global crt0_stack_low
	.align 4
crt0_stack_low:
	.space	8192
	.global crt0_stack_high
crt0_stack_high:
