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
	sub		%r0, %o0, %o0
2:
	retl
	nop

/* ==========================================================================
 * machdep_sys_fork()
 */
    .globl machdep_sys_fork;

machdep_sys_fork:;

    mov SYS_fork, %g1;
    ta 0;
    bcs 1b;
    nop;
    tst %o1
    bne,a __fork_parent
    mov  %g0, %o0
__fork_parent:;
    retl
    nop;	/* Added by monty to keep sparc assembler happy */

/* ========================================================================== 
 * Berkeley socket stuff
 * 
 * ==========================================================================
 * machdep_sys_socketcall()
 */
    .globl machdep_sys_socketcall;

machdep_sys_socketcall:;

	mov 83, %g1;   			
	ta 0;							
	bcs,a 2b;						
	sub %r0,%o0,%o0;				
	retl							
	nop;	/* Added by monty to keep sparc assembler happy */	
