/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_spin.c,v 11.6 2000/05/17 19:30:19 bostic Exp $";
#endif /* not lint */

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 */
int
__os_spin()
{
	SYSTEM_INFO SystemInfo;

	/*
	 * If the application specified a value or we've already figured it
	 * out, return it.
	 */
	if (DB_GLOBAL(db_tas_spins) != 0)
		return (DB_GLOBAL(db_tas_spins));

	/* Get the number of processors */
	GetSystemInfo(&SystemInfo);

	/*
	 * Spin 50 times per processor -- we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (SystemInfo.dwNumberOfProcessors > 1)
		 DB_GLOBAL(db_tas_spins) = 50 * SystemInfo.dwNumberOfProcessors;
	else
		 DB_GLOBAL(db_tas_spins) = 1;
	return (DB_GLOBAL(db_tas_spins));
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
	if (__db_jump.j_yield != NULL && __db_jump.j_yield() == 0)
		return;
	__os_sleep(dbenv, 0, usecs);
}
