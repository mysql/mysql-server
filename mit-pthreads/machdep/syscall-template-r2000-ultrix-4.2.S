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

#define SYSCALL(x)														\
																		\
	.globl	machdep_sys_##x;											\
	.ent	machdep_sys_##x;											\
																		\
machdep_sys_##x:;														\
																		\
	.frame	sp,0,ra;			/* No frame, return address in ra */	\
																		\
	li v0,SYS_##x;				/* Load syscall # into v0 */			\
	syscall;															\
	bne a3,zero,machdep_error;	/* Error if a3 != 0 */					\
	j ra;																\
																		\
	.end	machdep_sys_##x

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

#define XSYSCALL(NAME) SYSCALL(NAME)

XSYSCALL(SYSCALL_NAME)
