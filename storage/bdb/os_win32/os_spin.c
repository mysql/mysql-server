/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_spin.c,v 12.2 2005/07/20 16:52:02 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 */
u_int32_t
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	SYSTEM_INFO SystemInfo;
	u_int32_t tas_spins;

	/* Get the number of processors */
	GetSystemInfo(&SystemInfo);

	/*
	 * Spin 50 times per processor -- we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (SystemInfo.dwNumberOfProcessors > 1)
		 tas_spins = 50 * SystemInfo.dwNumberOfProcessors;
	else
		 tas_spins = 1;

	return (tas_spins);
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
