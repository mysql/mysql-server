/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_spin.c,v 11.13 2002/08/07 02:02:07 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#if defined(HAVE_PSTAT_GETDYNAMIC)
#include <sys/pstat.h>
#endif

#include <limits.h>
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
 *	Return the number of default spins before blocking.
 *
 * PUBLIC: int __os_spin __P((DB_ENV *));
 */
int
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * If the application specified a value or we've already figured it
	 * out, return it.
	 *
	 * XXX
	 * We don't want to repeatedly call the underlying function because
	 * it can be expensive (e.g., requiring multiple filesystem accesses
	 * under Debian Linux).
	 */
	if (dbenv->tas_spins != 0)
		return (dbenv->tas_spins);

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

	return (dbenv->tas_spins);
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
	(void)__os_sleep(dbenv, 0, usecs);
}
