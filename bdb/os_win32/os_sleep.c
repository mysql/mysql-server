/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_sleep.c,v 11.4 2000/03/30 01:46:43 ubell Exp $";
#endif /* not lint */

#include "db_int.h"
#include "os_jump.h"

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

	if (__db_jump.j_sleep != NULL)
		return (__db_jump.j_sleep(secs, usecs));

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 */
	Sleep(secs * 1000 + usecs / 1000);
	return (0);
}
