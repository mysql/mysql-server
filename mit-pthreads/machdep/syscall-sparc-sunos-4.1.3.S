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
    tst %o1
    bne,a __fork_parent
    mov  %g0, %o0
__fork_parent:;
    retl

/* POSIX-compliant getpgrp() takes no arguments.  The SunOS syscall wants
   one, and gives the POSIXy result if that argument is zero.  */
	.globl _getpgrp
_getpgrp:
	mov SYS_getpgrp, %g1
	mov 0, %i0
	ta 0
	bcs 1b
	nop
	retl
	nop

#if 0
/* I think this bit of magic will do the right thing for other syscalls.
   We get here with the new `errno' code in %o0.  It should get stored in
   *__error(), and -1 returned to the caller.  */
	.globl cerror
cerror:
	save %sp,-104,%sp
	/* Now value is in %i0.  Store it in *__error().  */
	call ___error
	nop
	st %i0,[%o0]

	/* Now also store a copy in global variable errno, for routines
	   like isatty that want to examine it and which haven't been
	   converted yet.  */
	sethi %hi(_errno), %o0
	st %i0,[%o0+%lo(_errno)]

#if 0 /* use this if you want -errno returned */
	sub %r0,%i0,%i0
#else /* return -1 */
	mov -1,%i0
#endif
	retl
	restore
#endif
