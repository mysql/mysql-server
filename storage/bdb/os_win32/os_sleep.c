/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_sleep.c,v 11.8 2002/07/12 18:56:56 bostic Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_sleep --
 *	Yield the processor for a period of time.
 */
int
__os_sleep(dbenv, secs, usecs)
	DB_ENV *dbenv;
	u_long secs, usecs;		/* Seconds and microseconds. */
{
	COMPQUIET(dbenv, NULL);

	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; ++secs, usecs -= 1000000)
		;

	if (DB_GLOBAL(j_sleep) != NULL)
		return (DB_GLOBAL(j_sleep)(secs, usecs));

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 */
	Sleep(secs * 1000 + usecs / 1000);
	return (0);
}
