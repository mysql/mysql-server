/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_sleep.c,v 11.23 2004/03/27 19:09:13 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_VXWORKS
#include <sys/times.h>
#include <time.h>
#include <selectLib.h>
#else
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH SYS_TIME */
#endif /* HAVE_VXWORKS */

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_sleep --
 *	Yield the processor for a period of time.
 *
 * PUBLIC: void __os_sleep __P((DB_ENV *, u_long, u_long));
 */
void
__os_sleep(dbenv, secs, usecs)
	DB_ENV *dbenv;
	u_long secs, usecs;		/* Seconds and microseconds. */
{
	struct timeval t;
	int ret;

	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; usecs -= 1000000)
		++secs;

	if (DB_GLOBAL(j_sleep) != NULL) {
		(void)DB_GLOBAL(j_sleep)(secs, usecs);
		return;
	}

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 *
	 * Sheer raving paranoia -- don't select for 0 time.
	 */
	t.tv_sec = (long)secs;
	if (secs == 0 && usecs == 0)
		t.tv_usec = 1;
	else
		t.tv_usec = (long)usecs;

	/*
	 * We don't catch interrupts and restart the system call here, unlike
	 * other Berkeley DB system calls.  This may be a user attempting to
	 * interrupt a sleeping DB utility (for example, db_checkpoint), and
	 * we want the utility to see the signal and quit.  This assumes it's
	 * always OK for DB to sleep for less time than originally scheduled.
	 */
	if (select(0, NULL, NULL, NULL, &t) == -1)
		 if ((ret = __os_get_errno()) != EINTR)
			__db_err(dbenv, "select: %s", strerror(ret));
}
