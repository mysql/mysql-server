/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_spin.c,v 11.11 2002/07/12 18:56:56 bostic Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 */
int
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	SYSTEM_INFO SystemInfo;

	/*
	 * If the application specified a value or we've already figured it
	 * out, return it.
	 */
	if (dbenv->tas_spins != 0)
		return (dbenv->tas_spins);

	/* Get the number of processors */
	GetSystemInfo(&SystemInfo);

	/*
	 * Spin 50 times per processor -- we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (SystemInfo.dwNumberOfProcessors > 1)
		 dbenv->tas_spins = 50 * SystemInfo.dwNumberOfProcessors;
	else
		 dbenv->tas_spins = 1;
	return (dbenv->tas_spins);
}

/*
 * __os_yield --
 *	Yield the processor.
 */
void
__os_yield(dbenv, usecs)
	DB_ENV *dbenv;
	u_long usecs;
{
	if (DB_GLOBAL(j_yield) != NULL && DB_GLOBAL(j_yield)() == 0)
		return;
	__os_sleep(dbenv, 0, usecs);
}
