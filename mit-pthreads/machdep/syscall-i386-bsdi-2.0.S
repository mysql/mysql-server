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
	.asciz "syscall-i386-netbsd-1.0.S,v 1.56 1995/09/26 21:04:05 raeburn Exp";
#endif
 
#if defined(SYSLIBC_SCCS) && !defined(lint)
        .asciz "@(#)syscall.s   5.1 (Berkeley) 4/23/90"
#endif /* SYSLIBC_SCCS and not lint */

#include <sys/syscall.h>

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
	.byte 0x9a; .long 0; .word 7;	
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
	.byte 0x9a; .long 0; .word 7;	
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


/*	BSDI DEFS.h,v 2.1 1995/02/03 06:28:24 polk Exp	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)DEFS.h	8.1 (Berkeley) 6/4/93
 */

#ifdef PROF
#define	ENTRY(x)	.globl _/**/x; .align 2; _/**/x:  \
			.data; 1:; .long 0; .text; \
			pushl %ebp; movl %esp,%ebp; \
			leal 1b,%eax; call mcount; leave
#define	ASENTRY(x)	.globl x; .align 2; x: \
			.data; 1:; .long 0; .text; \
			pushl %ebp; movl %esp,%ebp; \
			leal 1b,%eax; call mcount; leave
#else
#define	ENTRY(x)	.globl _/**/x; .align 2; _/**/x: 
#define	ASENTRY(x)	.globl x; .align 2; x: 
#endif

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "@(#)_setjmp.s	8.1 (Berkeley) 6/4/93"
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	_setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is NOT restored.
 */


.globl __setjmp;			/* Patch by Jan Legenhausen  (monty) */
.align 2;
__setjmp:
	movl	4(%esp),%eax
	movl	0(%esp),%edx
	movl	%edx, 0(%eax)		/* rta */
	movl	%ebx, 4(%eax)
	movl	%esp, 8(%eax)
	movl	%ebp,12(%eax)
	movl	%esi,16(%eax)
	movl	%edi,20(%eax)
	movl	$0,%eax
	ret

.globl __longjmp;			/* Patch by Jan Legenhausen ?  (monty) */
.align 2;
__longjmp:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	cmpl	$0,%eax
	jne	1f
	movl	$1,%eax
1:	movl	%ecx,0(%esp)
	ret
