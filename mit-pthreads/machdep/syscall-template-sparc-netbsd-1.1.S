/* ==== syscall.S ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 */

#include <sys/syscall.h>

#define	SYSCALL(x)						\
	.globl _machdep_sys_##x;			\
										\
_machdep_sys_##x:;						\
										\
	mov SYS_##x, %g1;   				\
	ta 0;								\
	bcs,a 2b;							\
	sub %r0,%o0,%o0;					\
	retl;								\
	nop

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
	sub		%r0, %o0, %o0
2:
	retl
	nop

#define XSYSCALL(NAME) SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
