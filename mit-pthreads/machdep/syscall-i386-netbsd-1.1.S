/* ==== syscall.S ============================================================
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 1993 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Description : Machine dependent syscalls for i386/i486/i586
 *
 *  1.00 93/08/26 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#ifndef lint
	.text
	.asciz "$Id$";
#endif
 
#if defined(SYSLIBC_SCCS) && !defined(lint)
        .asciz "@(#)syscall.s   5.1 (Berkeley) 4/23/90"
#endif /* SYSLIBC_SCCS and not lint */

#include <machine/asm.h>
#include <sys/syscall.h>

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2


/* ==========================================================================
 * machdep_sys_fork()
 */
	.globl _machdep_sys_fork;		
									
_machdep_sys_fork:;				
									
	movl $(SYS_fork), %eax;		
	.byte 0x9a; .long 0; .word 7;	
	cmpl $0, %edx
	je	2f
	movl $0, %eax
2:	
	ret;							

/* ==========================================================================
 * machdep_sys___syscall()
 */

_machdep_sys___syscall:;				

	movl $(SYS___syscall), %eax;		
	int $0x80;						
	jb 3f;
	ret

/* ==========================================================================
 * machdep_sys_sigsuspend()
 */
	.globl _machdep_sys_sigsuspend;		

_machdep_sys_sigsuspend:;

	movl    4(%esp),%eax            # fetch mask arg
    movl    (%eax),%eax             # indirect to mask arg
    movl    %eax,4(%esp)
	movl $(SYS_sigsuspend), %eax;		
	int $0x80;						
	jb 3f;
	ret

3:
	
	neg		%eax
	movl	$0xffffffff,%edx
	ret

/* ==========================================================================
 * machdep_sys_lseek()
 */
	.globl _machdep_sys_lseek;		

_machdep_sys_lseek:;				
									
	pushl  %ebp;
	movl   %esp,%ebp;
	pushl  0x14(%ebp);
	pushl  0x10(%ebp);
	pushl  0xc(%ebp);
	pushl  $0x0;
	pushl  0x8(%ebp);
	pushl  $0x0;
	pushl $(SYS_lseek);
	call _machdep_sys___syscall;
	leave
	ret

/* ==========================================================================
 * machdep_sys_ftruncate() ; Added by Monty
 */
        .globl _machdep_sys_ftruncate;
 
_machdep_sys_ftruncate:;
 
        pushl  %ebp; 
        movl   %esp,%ebp;
        pushl  0x10(%ebp);
        pushl  0xc(%ebp);
        pushl  $0x0;			# Why this?
        pushl  0x8(%ebp);
        pushl  $0x0;			# And this?
        pushl $(SYS_ftruncate);
        call _machdep_sys___syscall;
        leave
        ret  

/* ==========================================================================
 * machdep_sys_sigprocmask()
 */
	.globl _machdep_sys_sigprocmask;		

_machdep_sys_sigprocmask:;				
									
	movl 8(%esp),%ecx
	movl (%ecx),%ecx
    movl %ecx,8(%esp)
	movl $(SYS_sigprocmask), %eax;		
	int  $0x80;						
	jb   3b;
	ret

/* ==========================================================================
 * machdep_sys_pipe()
 */
	.globl _machdep_sys_pipe;		

_machdep_sys_pipe:;	
									
	movl $(SYS_pipe), %eax;		
	int  $0x80;						
	jb   3b;
	movl 4(%esp),%ecx
	movl %eax,(%ecx)
	movl %edx,4(%ecx)
	xorl %eax,%eax
	ret

