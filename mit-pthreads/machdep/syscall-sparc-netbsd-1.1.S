/* ==== syscall.S ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 */

#ifndef lint
	.text
	.asciz "$Id$";
#endif
 
#include <sys/syscall.h>

#define	SYSCALL(x)					\
	.globl _machdep_sys_##x;		\
									\
_machdep_sys_##x:;					\
									\
	mov SYS_##x, %g1;   			\
	ta 0;							\
	bcs,a 2b;						\
	sub %r0,%o0,%o0;				\
	retl							


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

/* ==========================================================================
 * machdep_sys_pipe()
 */
    .globl _machdep_sys_pipe

_machdep_sys_pipe:
    mov  %o0, %o2
    mov  SYS_pipe, %g1
    ta  0
    bcs 1b
    nop
    st  %o0, [ %o2 ]
    st  %o1, [ %o2 + 4 ]
    retl 
    mov  %g0, %o0

/* ==========================================================================
 * machdep_sys_fork()
 */
    .globl _machdep_sys_fork;

_machdep_sys_fork:;

    mov SYS_fork, %g1;
    ta 0;
    bcs 1b;
    nop;
	dec %o1;
    retl;
	and %o0, %o1, %o0;	! return 0 in child, pid in parent

/* ==========================================================================
 * machdep_sys_sigprocmask()
 */
    .globl _machdep_sys_sigprocmask;

_machdep_sys_sigprocmask:;

	ld [%o1], %o1; 
    mov SYS_sigprocmask, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
	nop

/* ==========================================================================
 * machdep_sys_sigsuspend()
 */
    .globl _machdep_sys_sigsuspend;

_machdep_sys_sigsuspend:;

	ld [%o0], %o0; 
    mov SYS_sigsuspend, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
	nop
