/* ==== syscall.S ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Machine dependent syscalls for decstation with r2000/r3000
 *
 *  1.00 93/08/14 proven
 *      -Started coding this file.
 */

	.text
    .ascii "$Id$";

#include <syscall.h>
#include <machine/regdef.h>

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2

/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value. Eventually I want to load the errno value directly
 * into pthread_run->error but until then ...
 */
machdep_error:
	negu v0, v0				/* Return negative of errno value. */
	j ra

/* ==========================================================================
 * Syscalls already done,
 * Standard		 		Other important		BSD sockets
 * fork 		=   2	select		=  93	socket		=  97
 * read 		=   3	readv		= 120	connect		=  98
 * write 		=   4	writev		= 121	accept		=  99
 * open			= 	5   getdirentries =     send		= 101
 * close		=   6						recv		= 102
 * creat		=	8						bind		= 104
 * link			=   9						listen		= 106
 * unlink		=  10						recvmsg		= 113
 * chdir		=  12						sendmsg		= 114
 * chmod		=  15						getsockopt  = 118
 * chown		=  16						recvfrom    = 125
 * lseek  		=  19						sendto		= 133
 * stat			=  38						shutdown	= 134
 * dup 			=  41						getpeername = 141
 * pipe 		=  42  					
 * execve		=  59  				
 * fstat 		=  62		
 * wait3		=  84	
 * dup2			=  90
 * fcntl		=  92
 * fchown		= 123
 * fchmod		= 124
 * rename		= 128
 * waitpid		= 189
 * ======================================================================= */

/* ==========================================================================
 * machdep_sys_fork()
 */
	.globl	machdep_sys_fork
	.ent	machdep_sys_fork

machdep_sys_fork:

	.frame	sp,0,ra				/* No frame, return address in ra */

	li v0,SYS_fork				/* Load fork syscall # into v0 */
	syscall					
	bne a3,zero,machdep_error	/* Error if a3 != 0 */
	beqz v1,__fork_parent		/* Second return value = 0, if parent */
	li v0,0
__fork_parent:
	j ra

	.end	machdep_sys_fork

/* ==========================================================================
 * machdep_sys_pipe()
 */
	.globl	machdep_sys_pipe
	.ent	machdep_sys_pipe

machdep_sys_pipe:

	.frame	sp,0,ra				/* No frame, return address in ra */

	li v0,SYS_pipe				/* Load pipe syscall # into v0 */
	syscall					
	bne a3,zero,machdep_error	/* Error if a3 != 0 */
	sw v0, 0(a0)
	sw v1, 4(a0)
	li v0, 0
	j ra

	.end	machdep_sys_pipe

/* ==========================================================================
 * Other important asm routines.
 * ======================================================================= */
/* ==========================================================================
 * fake_setjmp()
 */
	.globl	fake_setjmp
	.ent	fake_setjmp

fake_setjmp:

	.frame sp,0,ra				/* No frame, return address in ra */

								/* Save all the important registers */
	sw ra,8(a0)
	sw gp,124(a0)
	sw sp,128(a0)
	sw s0,76(a0)
								/* More registers needed. */
	j ra

	.end	fake_longjmp

/* ==========================================================================
 * machdep_sys_longjmp()
 */
	.globl	machdep_sys_longjmp
	.ent	machdep_sys_longjmp

machdep_sys_longjmp:

	.frame sp,0,ra				/* No frame, return address in ra */
	li a1,1						/* Load 1 into reg a1 */
	sw a1, 20(a0);				/* Load a1 into address a0 + 20 */
	li v0,SYS_sigreturn			/* Load sigreturn syscall # into v0 */
	syscall						/* Do syscall to do longjmp */
	j ra

	.end	machdep_sys_longjmp
