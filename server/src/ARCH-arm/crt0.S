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

#ifdef CRT0_PLATFORM_EARLY_INIT
#include CRT0_PLATFORM_EARLY_INIT
#else
.macro crt0_platform_early_init
.endm
#endif

#define MIMIC_A_VMLINUZ

.section .text.init,"ax"
.type _start,#function
.globl _start

#ifdef CONFIG_BOOTSTRAP_THUMB_ENTRY
.thumb
_start:
	adr r4, 1f
	bx  r4
1:
.arm
#else
_start:
#endif

/* Some bootloaders like it this way, for others it won't harm */
/* Only works on non-PIE builds because it leaves behind relocations in text
 * segment. */
#if defined(MIMIC_A_VMLINUZ) && !defined(CONFIG_BID_PIE)
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	b 10f
	.word 0x016f2818
	.word _start
	.word _module_data_end
10:
#endif

	// IRQs, FIQs off
	mrs r4, cpsr
	orr r4, #0xc0
	msr cpsr_c, r4

	crt0_platform_early_init

	/*
	 * It might be the case that we're not run at the position where we
	 * have been linked to. If this is the case we copy ourselves to the
	 * position we're linked to. If L4Re is built with PIE support we
	 * can run from anywhere and there is no need to copy ourselves around.
	 */
#ifndef CONFIG_BID_PIE
	adr	r4, run		/* Running version */
	ldr	r5, .LCrun	/* supposed to be version */
	cmp	r4, r5		/* If equal ... */
	beq	run		/* ... go to run */

	/* Disable caches as we're moving code around */
	mcr p15, 0, r6, c7, c5, 0 /* ICIALLU */
	mrc p15, 0, r6, c1, c0
	bic r6, #0x0004
	bic r6, #0x1000
	mcr p15, 0, r6, c1, c0

	/* Figure how to move */
        ldr     r7, crt_end_bin_lsb
	add     r7, r7, #3	/* Round up to word size for ldr and str... */
	bic     r7, r7, #3	/* ...below */
	subs	r8, r5, r4	/* r8 is the distance between the blocks */
	bpl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	r9, r5			/* r9: run address */
	ldr	r10, .LCstart_bin
	ldr	r6, 3f
	str	r6, [r10], #4
	ldr	r6, 32f
	str	r6, [r10], #4
	ldr	r6, 33f
	str	r6, [r10], #4
	ldr	r6, 34f
	str	r6, [r10], #4
	ldr	r6, 35f
	str	r6, [r10], #4
	ldr	pc, .LCstart_bin

3:	ldr	r6, [r4], #4
32:	str	r6, [r5], #4
33:	cmp	r5, r7
34:	blt	3b
35:	mov	pc, r9

	/* Copy behind, copy backwards */
move_behind:
	sub	r8, r7, r8	/* r8 points to the end of source image */
3:	ldr	r6, [r8, #-4]!	/* Take bytes */
	str	r6, [r7, #-4]!	/* Put bytes */
	cmp	r5, r7
	blt	3b
	ldr	pc, .LCrun


.LCrun:       .word run
.LCstart_bin: .word _start
crt_end_bin:     .word 0
crt_end_bin_lsb: .word _module_data_end;

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
#ifdef __ARM_FP
.fpu vfp
	/* Prevent triggering 'undefined instruction' exceptions by enabling
         * the FPU. Newer gcc versions implicitly generate FP instructions. */
	mrc	p15, 0, r8, c1, c0, 2     /* read CPACR */
	orr	r8, #(0xf << 20)          /* cp10 + cp11 (FPU) */
	mcr	p15, 0, r8, c1, c0, 2     /* try to enable */
	mrc	p15, 0, r8, c1, c0, 2     /* read CPACR */
	and	r8, #(0xf << 20)
	cmp	r8, #(0xf << 20)          /* verify enabled */
	bne	no_fpu
	vmrs	r8, fpexc
	orr	r8, r8, #(0x1 << 30)      /* enable FP unit */
	vmsr	fpexc, r8

no_fpu:
#endif

	adr	r8, do_bootstrap
	ldr	sp, .LCstack
	add	sp, sp, r8

#ifdef CONFIG_BID_PIE
	/* Save r0..r3. They are passed to __main and will be clobbered by
	 * reloc_static_pie. */
	mov v1, a1
	mov v2, a2
	mov v3, a3
	mov v4, a4

	adr a1, .L__ehdr_start_off
	ldr a2, .L__ehdr_start_off
	add a1, a1, a2
	bl reloc_static_pie

	mov a1, v1
	mov a2, v2
	mov a3, v3
	mov a4, v4
#endif

	bl	__main
1:	b	1b

.LCstack: .word (crt0_stack_high - do_bootstrap)
#ifdef CONFIG_BID_PIE
.L__ehdr_start_off:
	.word __ehdr_start - .L__ehdr_start_off
#endif

.section ".bss"

	.global crt0_stack_low
	.align 3
crt0_stack_low:
	.space	8192
	.global crt0_stack_high
crt0_stack_high:
