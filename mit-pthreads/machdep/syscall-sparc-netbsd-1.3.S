/* ==== syscall.S ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 */

#ifndef lint
	.text
	.asciz "$Id$";
#endif
 
#include <machine/asm.h>
#include <sys/syscall.h>

#define	SYSCALL(x)					\
	.globl _C_LABEL(machdep_sys_##x);		\
							\
_C_LABEL(machdep_sys_##x):;				\
							\
	mov SYS_##x, %g1;   				\
	ta 0;						\
	bcs,a 2b;					\
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
    .globl _C_LABEL(machdep_sys_pipe)

_C_LABEL(machdep_sys_pipe):
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
    .globl _C_LABEL(machdep_sys_fork);

_C_LABEL(machdep_sys_fork):;

    mov SYS_fork, %g1;
    ta 0;
    bcs 1b;
    nop;
    dec %o1;
    retl;
    and %o0, %o1, %o0;	! return 0 in child, pid in parent

#ifndef SYS___sigprocmask14
/* ==========================================================================
 * machdep_sys_sigprocmask()
 */
    .globl _C_LABEL(machdep_sys_sigprocmask);

_C_LABEL(machdep_sys_sigprocmask):;

    ld [%o1], %o1; 
    mov SYS_sigprocmask, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
    nop
#endif

#ifndef SYS___sigsuspend14
/* ==========================================================================
 * machdep_sys_sigsuspend()
 */
    .globl _C_LABEL(machdep_sys_sigsuspend);

_C_LABEL(machdep_sys_sigsuspend):;

    ld [%o0], %o0; 
    mov SYS_sigsuspend, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
    nop
#endif

/* ==========================================================================
 * machdep_sys_fstat()
 */
    .globl _C_LABEL(machdep_sys_fstat);

_C_LABEL(machdep_sys_fstat):;

    mov SYS___fstat13, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
    nop

/* ==========================================================================
 * machdep_sys___syscall()
 */
_machdep_sys___syscall:;

    mov SYS___syscall, %g1;
    ta 0;
    bcs 1b;
    nop;
    retl
    nop

/* ==========================================================================
 * machdep_sys_lseek()
 */
	.global _C_LABEL(machdep_sys_lseek)

_C_LABEL(machdep_sys_lseek):
	save %sp,-112,%sp
	mov %i1,%o4
	mov %i2,%o5
	st %i3,[%sp+92]
	mov 0,%o0
	mov SYS_lseek,%o1
	mov %i0,%o2
	call _machdep_sys___syscall,0
	mov 0,%o3
	mov %o0,%i0
	mov %o1,%i1
	ret
	restore

/* ==========================================================================
 * machdep_sys_ftruncate()
 */
	.global _C_LABEL(machdep_sys_ftruncate)

_C_LABEL(machdep_sys_ftruncate):
	save %sp,-104,%sp
	mov %i1,%o4
	mov %i2,%o5
	mov 0,%o0
	mov SYS_ftruncate,%o1
	mov %i0,%o2
	call _machdep_sys___syscall,0
	mov 0,%o3
	mov %o0,%o1
	sra %o0,31,%o0
	ret
	restore %g0,%o1,%o0
