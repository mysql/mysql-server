/* ==== syscall.S ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 */

#ifndef lint
	.text
	.asciz "$Id$";
#endif
 
#define _ASM
#include <sys/syscall.h>

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	4

/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	sub %r0, %o0, %o0
2:
	retl
	nop

#define concat2(a,b)	a ## b
#define concat(a,b)		concat2(a,b)
#define SYSval			concat(SYS_,SYSCALL_NAME)
#define procname		concat(machdep_sys_,SYSCALL_NAME)

	.globl procname

procname:
	mov SYSval, %g1
	ta 0
	bcs,a 2b
	sub %r0,%o0,%o0
	retl
	nop
