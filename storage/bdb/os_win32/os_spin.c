/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_spin.c,v 11.16 2004/03/24 15:13:16 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 */
void
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	SYSTEM_INFO SystemInfo;

	/*
	 * If the application specified a value or we've already figured it
	 * out, return it.
	 */
	if (dbenv->tas_spins != 0)
		return;

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
