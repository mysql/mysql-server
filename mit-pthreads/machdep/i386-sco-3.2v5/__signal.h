/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <sys/oldstyle/signal.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/signal.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <sys/posix/__signal.h>
#elif _STRICT_ANSI 	/* Pure Ansi/ISO environment */
#include <sys/ansi/signal.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <sys/ods_30_compat/signal.h>
#else 	/* Normal, default environment */
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

#pragma comment(exestr, "xpg4plus @(#) signal.h 20.3 94/12/19 ")

#ifndef _SIG_ATOMIC_T
#define _SIG_ATOMIC_T
   /* atomic entity for signal handling  */
typedef int sig_atomic_t;
#endif

extern const char * const _sys_siglist[];
extern const int _sys_nsig;

#ifndef	_SYS_SIGNAL_H
#include <sys/signal.h>
#endif

#define SignalBad		((SignalHandler)-1)
#define SignalDefault	((SignalHandler)0)
#define SignalIgnore	((SignalHandler)1)

#define	__sigmask(sig)		(1 << ((sig) - 1))
#define __SIGEMPTYSET	(~SIGALL)
#define	__SIGFILLSET	SIGALL
#define	__SIGADDSET(s,n)	((*s) |= (__sigmask(n)))
#define	__SIGDELSET(s,n)	((*s) &= ~(__sigmask(n))) 
#define	__SIGISMEMBER(s,n)	((*s) & (__sigmask(n))) 

#if !defined(_SYS_TYPES_H)
#include <sys/types.h>
#endif


#if __cplusplus
extern "C" {
#endif 

extern void (*signal(int, void(*)(int)))(int);
extern int raise(int);

extern void (*bsd_signal(int, void(*)(int)))(int);
extern  int ( *ssignal( int, int(*)(int) ) )(int);
extern  void ( *sigset( int, void(*)(int) ) )(int);
extern int killpg(pid_t, int);
#ifdef SS_ONSTACK	/* Not defined on old versions of the OS */
extern int sigaltstack(const stack_t *, stack_t *);
extern int sigstack(struct sigstack *, struct sigstack *);
#endif
extern int sighold(int);
extern int sigignore(int);
extern int siginterrupt(int, int);
extern int sigpause(int);
extern int sigrelse(int);

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

#endif
