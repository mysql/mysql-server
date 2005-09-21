/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_sleep.c,v 11.11 2004/03/24 15:13:16 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_sleep --
 *	Yield the processor for a period of time.
 */
void
__os_sleep(dbenv, secs, usecs)
	DB_ENV *dbenv;
	u_long secs, usecs;		/* Seconds and microseconds. */
{
	COMPQUIET(dbenv, NULL);

	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; ++secs, usecs -= 1000000)
		;

	if (DB_GLOBAL(j_sleep) != NULL) {
		DB_GLOBAL(j_sleep)(secs, usecs);
		return;
	}

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 */
	Sleep(secs * 1000 + usecs / 1000);
}
