/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)signal.h	8.3 (Berkeley) 3/30/94
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/__signal.h>

__BEGIN_DECLS

int	raise 			__P_((int));
/* RETSIGTYPE	signal	__P_((int __sig, RETSIGTYPE)); */

#ifndef	_ANSI_SOURCE

int	sigfillset 		__P_((sigset_t *));
int	sigemptyset		__P_((sigset_t *));
int	sigaddset 		__P_((sigset_t *, int));
int	sigdelset 		__P_((sigset_t *, int));
int	sigismember 	__P_((const sigset_t *, int));
int	sigsuspend 		__P_((const sigset_t *));
int	sigprocmask 	__P_((int, const sigset_t *, sigset_t *));

/* Still need work */
int	kill __P_((pid_t, int));
int	sigaction __P_((int, const struct sigaction *, struct sigaction *));
int	sigpending __P_((sigset_t *));

#ifndef _POSIX_SOURCE

int	killpg __P_((pid_t, int));
int	siginterrupt __P_((int, int));
void	psignal __P_((unsigned int, const char *));

/* int	sigpause __P_((int)); */
/* int	sigsetmask __P_((int)); */
/* int	sigblock __P_((int)); */
/* int	sigreturn __P_((struct sigcontext *)); */
/* int	sigvec __P_((int, struct sigvec *, struct sigvec *)); */
/* int	sigstack __P_((const struct sigstack *, struct sigstack *)); */

#endif	/* !_POSIX_SOURCE */
#endif	/* !_ANSI_SOURCE */

__END_DECLS

#endif	/* !_USER_SIGNAL_H */
