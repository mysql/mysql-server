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
	.asciz "syscall-i386-sco-3.2v5.S,v 1.2 1995/05/26 07:44:29 proven Exp";
#endif
 
#if defined(SYSLIBC_SCCS) && !defined(lint)
        .asciz "@(#)syscall.s   5.1 (Berkeley) 4/23/90"
#endif /* SYSLIBC_SCCS and not lint */

#include <sys/errno.h>
#include <sys/syscall.h>

#ifdef _SCO_ELF

#define NAME(X) 	machdep_sys_##X
#define GETADDR(X) \
	call 1f; \
1: \
	popl %ebx; \
	addl $NAME(X)+[.-1b], %ebx
	
#define END(X)		1: ; .type NAME(X),@function ; .size NAME(X),1b - NAME(X)

#else

#define NAME(X)		_machdep_sys_##X
#define END(X)

#endif

#ifdef __STDC__

#define	SYSCALL(x)					\
	.globl NAME(x);		\
									\
NAME(x):				\
	movl $(SYS_##x##), %eax;		\
	lcall	$7, $0; \
	jae	1f;							\
	cmp	$(ERESTART), %eax; \
	je	NAME(x); \
	neg		%eax;	\
1: \
	ret;							\
\
	END(x)


#else

#define	SYSCALL(x)					\
	.globl NAME(x);		\
									\
NAME(x):				\
									\
	movl $(SYS_/**/x), %eax;		\
	lcall	$7, $0; \
	jb	1b;							\
	ret;							\
	END(x)

#endif


/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2

/* ==========================================================================
 * machdep_sys_fork()
 */
	.globl NAME(fork);		

NAME(fork):;				
	movl $(SYS_fork), %eax
	lcall $7, $0
	jae   1f
	neg	%eax
	ret
1:
	test %edx, %edx
	je   2f
	xor  %eax, %eax
2:	
	ret;							
	END(fork)

/* ==========================================================================
 * NAME(getdents)
 */
	.globl NAME(getdents);		
									
NAME(getdents):
	movl $(SYS_getdents), %eax;		
	lcall	$7, $0
	jae   1f
	neg   %eax
1:
	ret;							
	END(getdents)

/* ==========================================================================
 * NAME(fxstat)
 */
	.globl NAME(fxstat);		
									
NAME(fxstat):;				
	movl $(SYS_fxstat), %eax;		
	lcall $7, $0
	jae   1f
	neg   %eax;
	ret;
1:
	xor  %eax, %eax;
	ret;							
	END(fxstat)

/* ==========================================================================
 * NAME(signal)
 */
	.globl NAME(signal);		
	
NAME(signal):;				
	movl	4(%esp), %ecx
	movl	$(SYS_signal),%eax
#if 0
	call	.L1
.L1:
	popl	%edx
	addl	$(NAME(sigreturn)+[.-.L1]), %edx
#else
	movl	$NAME(sigreturn), %edx
#endif
	lcall	$7,$0
	jae	1f
#	movl	%eax, 12(%esp)
	neg	%eax
	ret
1:
#	xor	%eax, %eax
#	movl	%eax, 12(%esp)
	ret
	END(signal)

/* ==========================================================================
 * NAME(sigaction)
 */
	.globl NAME(sigaction);		
									
NAME(sigaction):
	movl	$(SYS_sigaction),%eax
#if 0
	call	.L2
.L2:
	popl	%edx
	addl	$(NAME(sigreturn)+[.-.L2]), %edx
#else
	movl	$NAME(sigreturn), %edx
#endif
	lcall	$7, $0
	jb	1f
	xor	%eax, %eax
	ret
1:
	neg	%eax
	ret
	END(sigaction)

	.globl	NAME(sigreturn)
NAME(sigreturn):
        addl    $4,%esp
	lcall	$0xf, $0
	nop
	nop
	END(sigreturn)

/* ==========================================================================
 * NAME(waitpid)
 */
	.globl NAME(waitpid);		
									
NAME(waitpid):
        .byte   0x9c
	popl	%eax
	orl	$0x8c4, %eax
	pushl	%eax
	.byte	0x9d
	movl	$(SYS_wait), %eax
	lcall	$7, $0
	jae	2f
	cmpl	$(ERESTART), %al
	je	NAME(waitpid)
	neg	%eax
3:
	ret
2:
	movl	8(%esp), %ecx
	test	%ecx,%ecx
	je	3b
	mov	%edx, (%ecx)
	ret
	END(waitpid)

/* ==========================================================================
 * NAME(uname)
 */
	.globl NAME(uname);		
									
NAME(uname):
	pushl	$0
	pushl	$0
	pushl	12(%esp)
	subl	$4, %esp
	movl	$(SYS_utssys), %eax
	.byte 0x9a; .long 0; .word 7;	
	jb	1f
	addl	$16, %esp
	ret
1:
	addl	$16, %esp
	neg	%eax
        ret
	END(uname)


/* ==========================================================================
 * machdep_sys_ftruncate
 */

SYSCALL(ftruncate)

/* ==========================================================================
 * machdep_sys_ftime
 */

SYSCALL(ftime)

/* ==========================================================================
 * machdep_sys_gettimeofday()
 */

SYSCALL(gettimeofday)

/* ==========================================================================
 * machdep_sys_kill()
 */

SYSCALL(kill)

/* ==========================================================================
 * pthread_sys_setitimer
 */
	.globl NAME(setitimer);		
									
NAME(setitimer):;				
	movl $(SYS_setitimer), %eax;		
	lcall	$7, $0
	jae   1f
	neg   %eax
	ret
1:
	xor  %eax, %eax
	ret;	
	END(setitimer)						

/* ==========================================================================
 * pthread_sys_sysconf
 */
	.globl NAME(sysconf);		
									
NAME(sysconf):;				
	movl $(SYS_sysconf), %eax;		
	lcall $7, $0
	jae   1f
	neg   %eax
1:
	ret;	
	END(sysconf)						

/* ==========================================================================
 * pthread_sys_sysi86()
 */
	.globl NAME(sysi86);		
									
NAME(sysi86):;				
	movl $(SYS_sysi86), %eax
	lcall $7, $0
	jae   1f
	neg   %eax
1:
	ret;							
	END(sysi86)


/* ==========================================================================
 * machdep_sys_brk()
 */
	.globl NAME(brk);		
									
NAME(brk):;				
	movl $(SYS_break), %eax
	lcall $7, $0
	jae   1f
	neg   %eax
	ret
1:
	xor	%eax, %eax
	ret;							
	END(brk)

/* ==========================================================================
 * machdep_sys_pipe()
 */
	.globl NAME(pipe);		
									
NAME(pipe):;				
	movl $(SYS_pipe), %eax;		
	lcall $7, $0
	jae   1f
	neg   %eax
	ret
1:
	movl	4(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	xor	%eax, %eax
	ret;							
	END(brk)

/* ==========================================================================
 * machdep_sys_fcntl()
 */
	.globl NAME(fcntl);		
									
NAME(fcntl):;				
	movl $(SYS_fcntl), %eax
	lcall $7, $0
	jae   1f
	neg   %eax
1:
	ret
	END(fcntl)


/* ==========================================================================
 * machdep_sys_select()
 */
	.globl NAME(select);		
									
NAME(select):;				
	movl $(SYS_select), %eax
	lcall $7, $0
	jae   1f
	cmp   $(ERESTART), %eax
	jne   2f
	movl  $(EINTR), %eax
2:
	neg   %eax
1:
	ret
	END(select)


/* ==========================================================================
 * setjmp()
 */
	.globl setjmp;		
									
setjmp:
	movl	4(%esp),%eax
	movl	%ebx,(%eax)
	movl	%esi,4(%eax)
	movl	%edi,8(%eax)
	movl	%ebp,12(%eax)
	popl	%edx
	movl	%esp,16(%eax)
	movl	%edx,20(%eax)
	subl	%eax,%eax
	jmp	*%edx
1: ; 	.type setjmp,@function ; .size setjmp,1b - setjmp

/* ==========================================================================
 * longjmp()
 */
	.globl longjmp;		
									
longjmp:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ebx
	movl	4(%edx),%esi
	movl	8(%edx),%edi
	movl	12(%edx),%ebp
	movl	16(%edx),%esp
	test	%eax,%eax
	jne	1f
	inc	%eax
1:
	jmp	*20(%edx)
1: ; 	.type longjmp,@function ; .size longjmp,1b - longjmp
