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
#ifdef __PIC__
	lea	_stack(%rip),%esp
#else
	lea	_stack,%esp
#endif

#ifdef __PIC__
	pushq	$_exit@plt
	jmp	__main@plt
#else
	pushq	$_exit
	jmp	__main
#endif


	/* MultiBoot header - see multiboot.h. */
	.p2align(2)

#define MULTIBOOT_MEMORY_INFO 0x00000002

_mb_header:
	.long	0x1BADB002		/* magic */
	.long	MULTIBOOT_MEMORY_INFO	/* flags */
	.long	0 - 0x1BADB002 - MULTIBOOT_MEMORY_INFO

	.bss

	.globl  _stack
	.space	8192
_stack:
