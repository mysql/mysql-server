/*	$NetBSD: wait.h,v 1.7 1994/06/29 06:46:23 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)wait.h	8.1 (Berkeley) 6/2/93
 */

/*
 * This file holds definitions relevent to the wait4 system call
 * and the alternate interfaces that use it (wait, wait3, waitpid).
 */

/*
 * Macros to test the exit status returned by wait and extract the
 * relevant values. Union wait is no supported with pthreads.
 */
#define	__W_INT(i)			(i)
#define	__WSTATUS(x)		(__W_INT(x) & 0177)
#define	__WSTOPPED			0177		/* __WSTATUS if process is stopped */
#define WIFSTOPPED(x)		(__WSTATUS(x) == __WSTOPPED)
#define WSTOPSIG(x)			(__W_INT(x) >> 8)
#define WIFSIGNALED(x)		(__WSTATUS(x) != __WSTOPPED && __WSTATUS(x) != 0)
#define WTERMSIG(x)			(__WSTATUS(x))
#define WIFEXITED(x)		(__WSTATUS(x) == 0)
#define WEXITSTATUS(x)		(__W_INT(x) >> 8)

#ifndef _POSIX_SOURCE
#define WCOREDUMP(x)		(__W_INT(x) & WCOREFLAG)
#define	W_EXITCODE(ret, sig)	((ret) << 8 | (sig))
#define	W_STOPCODE(sig)		((sig) << 8 | __WSTOPPED)
#endif

/*
 * Option bits for the third argument of wait4.  WNOHANG causes the
 * wait to not hang if there are no stopped or terminated processes, rather
 * returning an error indication in this case (pid==0).  WUNTRACED
 * indicates that the caller should receive status about untraced children
 * which stop due to signals.  If children are stopped and a wait without
 * this option is done, it is as though they were still running... nothing
 * about them is returned.
 */
#define WNOHANG		1	/* dont hang in wait */
#define WUNTRACED	2	/* tell about stopped, untraced children */

#ifndef _POSIX_SOURCE

/* Tokens for special values of the "pid" parameter to wait4. */
#define	WAIT_ANY			(-1)	/* any process */
#define	WAIT_MYPGRP			0		/* any process in my process group */

#define	WSTOPPED			__WSTOPPED
#endif /* _POSIX_SOURCE */

#include <sys/types.h>
#include <sys/cdefs.h>

#ifndef __WAIT_STATUS
#define __WAIT_STATUS	int *
#endif

__BEGIN_DECLS
pid_t	wait 		__P_((int *));
pid_t	waitpid 	__P_((pid_t, int *, int));
#ifndef _POSIX_SOURCE
pid_t	wait3 		__P_((int *, int, void *));
pid_t	wait4 		__P_((pid_t, int *, int, void *));
#endif
__END_DECLS
