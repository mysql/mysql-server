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
	.align	0


/* ==========================================================================
 * machdep_sys_fork()
 */
	.globl _C_LABEL(machdep_sys_fork);		
									
_C_LABEL(machdep_sys_fork):;				

	swi	SYS_fork
	bcs	1f
        sub     r1, r1, #0x00000001
	and     r0, r0, r1
	mov     r15, r14

									

/* ==========================================================================
 * machdep_sys_fstat()
 */
	.globl _C_LABEL(machdep_sys_fstat);

_C_LABEL(machdep_sys_fstat):;				

	swi     SYS___fstat13
	bcs     1f
	mov     r15, r14

/* ==========================================================================
 * machdep_sys___syscall()
 */

_machdep_sys___syscall:;				

	swi     SYS___syscall
	bcs     1f
	mov     r15, r14


#ifndef SYS___sigsuspend14
/* ==========================================================================
 * machdep_sys_sigsuspend()
 */
	.globl _C_LABEL(machdep_sys_sigsuspend);		

_C_LABEL(machdep_sys_sigsuspend):;

	ldr     r0, [r0]
	swi     SYS_sigsuspend
	bcs     1f
	mov     r0, #0x00000000
	mov     r15, r14

#endif

#ifndef SYS___sigprocmask14
/* ==========================================================================
 * machdep_sys_sigprocmask()
 */
	.globl _C_LABEL(machdep_sys_sigprocmask);		

_C_LABEL(machdep_sys_sigprocmask):;				
									
	teq     r1, #0x00000000
	moveq   r0, #0x00000001
	moveq   r1, #0x00000000
	ldrne   r1, [r1]
	swi     SYS_sigprocmask
	bcs     1f
	teq     r2, #0x00000000
	strne   r0, [r2]
	mov     r0, #0x00000000
	mov     r15, r14
#endif

/* ==========================================================================
 * machdep_sys_ftruncate()
 */
	.global	_C_LABEL(machdep_sys_ftruncate)
_C_LABEL(machdep_sys_ftruncate):
	mov	r12, r13
	stmfd	r13!, {r11, r12, r14, r15}
	sub	r13, r13, #8
	stmia	r13, {r1-r2}
	mov	r3, #0
	sub	r11, r12, #4
	mov	r2, r0
	mov	r1, #0
	mov	r0, #201
	bl	_machdep_sys___syscall
	ldmea	r11, {r11, r13, r15}

/* ==========================================================================
 * machdep_sys_lseek()
 */
	.global	_C_LABEL(machdep_sys_lseek)

_C_LABEL(machdep_sys_lseek):
	mov	r12, r13
	stmfd	r13!, {r11, r12, r14, r15}
	str	r3, [r13, #-4]!
	sub	r13, r13, #8
	stmia	r13, {r1-r2}
	sub	r11, r12, #4
	mov	r3, #0
	mov	r2, r0
	mov	r1, #0
	mov	r0, #SYS_lseek
	bl	_machdep_sys___syscall
	ldmea	r11, {r11, r13, r15}

/* ==========================================================================
 * machdep_sys_pipe()
 */
	.globl _C_LABEL(machdep_sys_pipe);		

_C_LABEL(machdep_sys_pipe):;	

        mov     r2, r0
	swi     SYS_pipe
	bcs     1f
	str     r0, [r2, #0x0000]
	str     r1, [r2, #0x0004]
	mov     r0, #0x00000000
	mov     r15, r14


1:
	rsb	r0, r0, #0x00000000
	mvn	r1, #0x00000000
	mov	r15, r14
