/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_spin.c,v 11.20 2004/06/23 14:10:56 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#if defined(HAVE_PSTAT_GETDYNAMIC)
#include <sys/pstat.h>
#endif

#include <limits.h>			/* Needed for sysconf on Solaris. */
#include <unistd.h>
#endif

#include "db_int.h"

#if defined(HAVE_PSTAT_GETDYNAMIC)
static int __os_pstat_getdynamic __P((void));

/*
 * __os_pstat_getdynamic --
 *	HP/UX.
 */
static int
__os_pstat_getdynamic()
{
	struct pst_dynamic psd;

	return (pstat_getdynamic(&psd,
	    sizeof(psd), (size_t)1, 0) == -1 ? 1 : psd.psd_proc_cnt);
}
#endif

#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
static int __os_sysconf __P((void));

/*
 * __os_sysconf --
 *	Solaris, Linux.
 */
static int
__os_sysconf()
{
	long nproc;

	return ((nproc = sysconf(_SC_NPROCESSORS_ONLN)) > 1 ? (int)nproc : 1);
}
#endif

/*
 * __os_spin --
 *	Set the number of default spins before blocking.
 *
 * PUBLIC: void __os_spin __P((DB_ENV *));
 */
void
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * If the application specified a value or we've already figured it
	 * out, return it.
	 *
	 * Don't repeatedly call the underlying function because it can be
	 * expensive (for example, taking multiple filesystem accesses under
	 * Debian Linux).
	 */
	if (dbenv->tas_spins != 0)
		return;

	dbenv->tas_spins = 1;
#if defined(HAVE_PSTAT_GETDYNAMIC)
	dbenv->tas_spins = __os_pstat_getdynamic();
#endif
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
	dbenv->tas_spins = __os_sysconf();
#endif

	/*
	 * Spin 50 times per processor, we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (dbenv->tas_spins != 1)
		dbenv->tas_spins *= 50;
}

/*
 * __os_yield --
 *	Yield the processor.
 *
 * PUBLIC: void __os_yield __P((DB_ENV*, u_long));
 */
void
__os_yield(dbenv, usecs)
	DB_ENV *dbenv;
	u_long usecs;
{
	if (DB_GLOBAL(j_yield) != NULL && DB_GLOBAL(j_yield)() == 0)
		return;
#ifdef HAVE_VXWORKS
	taskDelay(1);
#endif
	__os_sleep(dbenv, 0, usecs);
}
