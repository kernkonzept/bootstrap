/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021, 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#include <l4/bid_config.h>
#include <sys/asm.h>

.section ".text.init", "ax"
.type _start, @function
.globl _start
_start:

.option push
.option norelax
  la gp, __global_pointer$
.option pop

  // Arguments provided by firmware (OpenSBI)
  // a0: Hart ID
  // a1: Flattened Device Tree

  // Store hartid in tp register
  mv tp, a0

  la t1, _mp_hart_lottery
  li t0, 1
  amoadd.w t1, t0, (t1)

  // First hart continues with bootstrap
  bnez t1, _wait_for_bootstrap

  la sp, crt0_stack_high

#ifdef CONFIG_BID_PIE
  /* Calculate load address... subtract link address from real one */
  adrp    x0, :got:_start
  ldr     x0, [x0, #:got_lo12:_start]
  adr     x1, _start
  sub     x0, x1, x0

  /* Do relocations */
  bl reloc_static_pie
#endif

  tail __main

_wait_for_bootstrap:
  // Other harts wait for bootstrap to finish
  REG_L t0, (mp_launch_boot_kernel)
  beqz t0, _wait_for_bootstrap

  // Continue with next stage
  jr t0


.section ".bss", "aw"

.global crt0_stack_low
  .balign 16
crt0_stack_low:
  .space 8192
  .global crt0_stack_high
crt0_stack_high:


.section ".data", "aw"

.global mp_launch_boot_kernel
  .balign 8
mp_launch_boot_kernel:
  .zero 8

_mp_hart_lottery:
  .zero 4
