/* ==== syscall.S ============================================================
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 1995 Chris Provenzano, proven@mit.edu
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
 */

#ifndef lint
	.text
	.asciz "syscall-i386-freebsd-2.0.S,v 1.2 1995/05/26 07:44:29 proven Exp";
#endif
 
#if defined(SYSLIBC_SCCS) && !defined(lint)
        .asciz "@(#)syscall.s   5.1 (Berkeley) 4/23/90"
#endif /* SYSLIBC_SCCS and not lint */

#include <sys/syscall.h>

#ifdef __STDC__

#ifdef __ELF__
#define NAME(X)		machdep_sys_##X
#else
#define NAME(X)		_machdep_sys_##X
#endif

#else

#ifdef __ELF__
#define NAME(X)		machdep_sys_/**/X
#else
#define NAME(X)		_machdep_sys_/**/X
#endif

#endif

#ifdef __ELF__
#define END(X)		5: ; .type NAME(X),@function ; .size NAME(X),5b - NAME(X)
#define KERNCALL	int $0x80
#else
#define END(X)
#define KERNCALL	.byte 0x9a; .long 0; .word 7;
#endif

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	neg		%eax
	ret

/* ==========================================================================
 * machdep_sys_fork()
 */
	.globl NAME(fork);		
									
NAME(fork):;				
									
	movl $(SYS_fork), %eax;		
	KERNCALL;
	cmpl $0, %edx
	je	2f
	movl $0, %eax
2:	
	ret;							
	END(fork)

/* ==========================================================================
 * machdep_sys___syscall()
 */

_machdep_sys___syscall:;				

	movl $(SYS___syscall), %eax;		
	KERNCALL;
	jb 3f;
	ret

/* ==========================================================================
 * machdep_sys_sigsuspend()
 */
	.globl NAME(sigsuspend);		

NAME(sigsuspend):;

	movl    4(%esp),%eax            # fetch mask arg
	movl    (%eax),%eax             # indirect to mask arg
	movl    %eax,4(%esp)
	movl $(SYS_sigsuspend), %eax;		
	KERNCALL;
	jb 3f;
	ret
3:
	neg		%eax
	movl	$0xffffffff,%edx
	ret
	END(sigsuspend)

/* ==========================================================================
 * machdep_sys_sigprocmask()
 */
	.globl NAME(sigprocmask);		

NAME(sigprocmask):;

	movl    8(%esp),%ecx
	movl    (%ecx),%ecx
	movl    %ecx,8(%esp)
	movl    $ SYS_sigprocmask , %eax
	KERNCALL;
	jb 4f;
	ret
4:
	neg     %eax
	movl    $0xffffffff,%edx
	ret
	END(sigprocmask)

/* ==========================================================================
 * machdep_sys_lseek()
 */
	.globl NAME(lseek);		

NAME(lseek):;				
									
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
	END(lseek)

/* ==========================================================================
 * machdep_sys_ftruncate() ; Added by Monty
 */
        .globl NAME(ftruncate);
 
NAME(ftruncate):;
 
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
	END(ftruncate)

/* ==========================================================================
 * machdep_sys_setjmp()
 */
	.globl NAME(setjmp);		

NAME(setjmp):;				
	movl    4(%esp),%eax
	movl    0(%esp),%edx
	movl    %edx, 0(%eax)       /* rta */
	movl    %ebx, 4(%eax)
	movl    %esp, 8(%eax)
	movl    %ebp,12(%eax)
	movl    %esi,16(%eax)
	movl    %edi,20(%eax)
	xorl    %eax,%eax
	ret
	END(setjmp)

/* ==========================================================================
 * machdep_sys_longjmp()
 */
    .globl NAME(longjmp);
    
NAME(longjmp):;
	movl    4(%esp),%edx
	movl    8(%esp),%eax
	movl    0(%edx),%ecx
	movl    4(%edx),%ebx
	movl    8(%edx),%esp
	movl    12(%edx),%ebp
	movl    16(%edx),%esi
	movl    20(%edx),%edi
	testl   %eax,%eax
	jnz 1f
	incl    %eax
1:
	movl    %ecx,0(%esp)
	ret
	END(longjmp)
