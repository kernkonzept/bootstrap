/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
/* -*- c -*- */

.section .text.init, "ax"
.type _start, @function
.globl _start
_start:
  lis  %r1, crt0_stack_high@ha  /* load stack pointer */
  addi %r1, %r1, crt0_stack_high@l
  b    __main
1:
  b 1b

.section ".bss", "aw"

.global crt0_stack_low
crt0_stack_low:
  .space 4096
.global crt0_stack_high
crt0_stack_high:
