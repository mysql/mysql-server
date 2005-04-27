/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_clock.c,v 1.9 2002/03/29 20:46:44 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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

#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 *
 * PUBLIC: int __os_clock __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
int
__os_clock(dbenv, secsp, usecsp)
	DB_ENV *dbenv;
	u_int32_t *secsp, *usecsp;	/* Seconds and microseconds. */
{
#if defined(HAVE_GETTIMEOFDAY)
	struct timeval tp;
	int ret;

retry:	if (gettimeofday(&tp, NULL) != 0) {
		if ((ret = __os_get_errno()) == EINTR)
			goto retry;
		__db_err(dbenv, "gettimeofday: %s", strerror(ret));
		return (ret);
	}

	if (secsp != NULL)
		*secsp = tp.tv_sec;
	if (usecsp != NULL)
		*usecsp = tp.tv_usec;
#endif
#if !defined(HAVE_GETTIMEOFDAY) && defined(HAVE_CLOCK_GETTIME)
	struct timespec tp;
	int ret;

retry:	if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
		if ((ret = __os_get_errno()) == EINTR)
			goto retry;
		__db_err(dbenv, "clock_gettime: %s", strerror(ret));
		return (ret);
	}

	if (secsp != NULL)
		*secsp = tp.tv_sec;
	if (usecsp != NULL)
		*usecsp = tp.tv_nsec / 1000;
#endif
#if !defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_CLOCK_GETTIME)
	time_t now;
	int ret;

	if (time(&now) == (time_t)-1) {
		ret = __os_get_errno();
		__db_err(dbenv, "time: %s", strerror(ret));
		return (ret);
	}

	if (secsp != NULL)
		*secsp = now;
	if (usecsp != NULL)
		*usecsp = 0;
#endif
	return (0);
}
