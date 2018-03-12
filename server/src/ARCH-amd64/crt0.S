/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/compiler.h>
	
	.section .init

	.globl	_start
_start:
#if defined(REALMODE_LOADING) && !defined(IMAGE_MODE)
	cld
	cli
	mov	$(3 * 8), %eax
	mov	%eax, %ds
	mov	%eax, %es
	mov	%eax, %fs
	mov	%eax, %gs

	lss	_stack_seg, %esp
#else
#ifdef __PIC__
	lea	_stack(%rip),%esp
#else
	lea	_stack,%esp
#endif
#endif

#ifdef __PIC__
	push	$_exit@plt
	jmp	__main@plt
#else
	push	$_exit
	jmp	__main
#endif


        /* MultiBoot header - see multiboot.h.  */
	.p2align(2)

_mb_header:
        .long   0x1BADB002		/* magic */
        .long   0			/* flags: AOUT_KLUDGE */
        .long   0 - 0x1BADB002

#if defined(REALMODE_LOADING) && !defined(IMAGE_MODE)
_stack_seg:
	.long	_stack
	.word	(3 * 8)
#endif

	.bss

	.globl  _stack
	.space	8192
_stack:
