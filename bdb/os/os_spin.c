/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_spin.c,v 11.5 2000/03/30 01:46:42 ubell Exp $";
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
#include "os_jump.h"

#if defined(HAVE_PSTAT_GETDYNAMIC)
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
/*
 * __os_sysconf --
 *	Solaris, Linux.
 */
static int
__os_sysconf()
{
	int nproc;

	return ((nproc = sysconf(_SC_NPROCESSORS_ONLN)) > 1 ? nproc : 1);
}
#endif

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 *
 * PUBLIC: int __os_spin __P((void));
 */
int
__os_spin()
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
	if (DB_GLOBAL(db_tas_spins) != 0)
		return (DB_GLOBAL(db_tas_spins));

	DB_GLOBAL(db_tas_spins) = 1;
#if defined(HAVE_PSTAT_GETDYNAMIC)
	DB_GLOBAL(db_tas_spins) = __os_pstat_getdynamic();
#endif
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
	DB_GLOBAL(db_tas_spins) = __os_sysconf();
#endif

	/*
	 * Spin 50 times per processor, we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (DB_GLOBAL(db_tas_spins) != 1)
		DB_GLOBAL(db_tas_spins) *= 50;

	return (DB_GLOBAL(db_tas_spins));
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
	if (__db_jump.j_yield != NULL && __db_jump.j_yield() == 0)
		return;
	__os_sleep(dbenv, 0, usecs);
}
