/*
 *   Portions Copyright (C) 1983-1995 The Santa Cruz Operation, Inc.
 *		All Rights Reserved.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of The Santa Cruz Operation, Inc.  Such users have the
 *	right to use, modify, and incorporate this code into other products
 *	for purposes authorized by the license agreement provided they include
 *	this notice and the associated copyright notice with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 */

/*	Portions Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc. */
/*	Portions Copyright (c) 1979 - 1990 AT&T   */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*	UNIX System Laboratories, Inc.                          */
/*	The copyright notice above does not evidence any        */
/*	actual or intended publication of such source code.     */

#ifndef ___SIGNAL_H
#define ___SIGNAL_H

#pragma comment(exestr, "posix @(#) signal.h 20.3 94/12/19 ")

#ifndef _SIG_ATOMIC_T
#define _SIG_ATOMIC_T
   /* atomic entity for signal handling  */
typedef int sig_atomic_t;
#endif


#ifndef	_SYS_SIGNAL_H
#include <sys/signal.h>
#endif

#if !defined(_SYS_TYPES_H)
#include <sys/types.h>
#endif


#if __cplusplus
extern "C" {
#endif 

extern void (*signal(int, void(*)(int)))(int);
extern int raise(int);



extern int (sigfillset)(sigset_t *);
extern int (sigemptyset)(sigset_t *);
extern int (sigaddset)(sigset_t *, int);
extern int (sigdelset)(sigset_t *, int);
extern int (sigismember)(const sigset_t *, int);
extern int sigpending(sigset_t *);
extern int sigsuspend(const sigset_t *);
extern int sigprocmask(int, const sigset_t *, sigset_t *);
extern int kill(pid_t, int);
extern int sigaction(int, const struct sigaction *, struct sigaction *);

#if __cplusplus
};
#endif 


#endif /* ___SIGNAL_H  */

