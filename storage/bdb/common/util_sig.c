/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: util_sig.c,v 1.9 2004/01/28 03:35:54 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <signal.h>
#endif

#include "db_int.h"

static int	interrupt;
static void	onint __P((int));

/*
 * onint --
 *	Interrupt signal handler.
 */
static void
onint(signo)
	int signo;
{
	if ((interrupt = signo) == 0)
		interrupt = SIGINT;
}

/*
 * __db_util_siginit --
 *
 * PUBLIC: void __db_util_siginit __P((void));
 */
void
__db_util_siginit()
{
	/*
	 * Initialize the set of signals for which we want to clean up.
	 * Generally, we try not to leave the shared regions locked if
	 * we can.
	 */
#ifdef SIGHUP
	(void)signal(SIGHUP, onint);
#endif
	(void)signal(SIGINT, onint);
#ifdef SIGPIPE
	(void)signal(SIGPIPE, onint);
#endif
	(void)signal(SIGTERM, onint);
}

/*
 * __db_util_interrupted --
 *	Return if interrupted.
 *
 * PUBLIC: int __db_util_interrupted __P((void));
 */
int
__db_util_interrupted()
{
	return (interrupt != 0);
}

/*
 * __db_util_sigresend --
 *
 * PUBLIC: void __db_util_sigresend __P((void));
 */
void
__db_util_sigresend()
{
	/* Resend any caught signal. */
	if (interrupt != 0) {
		(void)signal(interrupt, SIG_DFL);
		(void)raise(interrupt);
		/* NOTREACHED */
	}
}
