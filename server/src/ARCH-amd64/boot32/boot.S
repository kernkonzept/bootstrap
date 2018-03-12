/*
 * (c) 2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/* -*- c -*- */

	.globl	_start
	.p2align 4
_start:
#ifdef REALMODE_LOADING
	cld
	cli
	mov	$(3 * 8), %eax
	mov	%eax, %ds
	mov	%eax, %es
	mov	%eax, %fs
	mov	%eax, %gs

	/* We have the following problem: Our image is loaded at 0x00100000
	 * by the boot loader but we don't want to get into conflict with
	 * the AMD64 executable. Therefore we move ourself out of the way.
	 * We are linked to 0x01000000 (see ARCH-amd64/boot32/bootstrap.ld) */
	mov	%esi, %ebx
	mov	$0x00100000, %esi
	mov	$0x01000000, %edi
	mov	$_image_end, %ecx
	sub	$_image_start, %ecx
	rep	movsb
	mov	$1f, %eax
	jmp	*%eax
1:	mov	%ebx, %esi
	lss	_stack_seg, %esp
#else
	leal	_stack,%esp
#endif

	pushl	%esi			/* ptr to real mode */
	pushl	%eax
	pushl	%ebx
	pushl	$0			/* no return address */
	jmp	bootstrap

        .align  4

	.p2align(2), 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

#ifdef REALMODE_LOADING
_stack_seg:
	.long	_stack
	.word	(3 * 8)
#endif

	.bss
	.space	8192
_stack:
