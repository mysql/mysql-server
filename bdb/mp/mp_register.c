/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_register.c,v 11.12 2000/11/15 19:25:39 sue Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

/*
 * memp_register --
 *	Register a file type's pgin, pgout routines.
 */
int
memp_register(dbenv, ftype, pgin, pgout)
	DB_ENV *dbenv;
	int ftype;
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
{
	DB_MPOOL *dbmp;
	DB_MPREG *mpreg;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_register(dbenv, ftype, pgin, pgout));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->mp_handle, DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;

	/*
	 * Chances are good that the item has already been registered, as the
	 * DB access methods are the folks that call this routine.  If already
	 * registered, just update the entry, although it's probably unchanged.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	for (mpreg = LIST_FIRST(&dbmp->dbregq);
	    mpreg != NULL; mpreg = LIST_NEXT(mpreg, q))
		if (mpreg->ftype == ftype) {
			mpreg->pgin = pgin;
			mpreg->pgout = pgout;
			break;
		}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
	if (mpreg != NULL)
		return (0);

	/* New entry. */
	if ((ret = __os_malloc(dbenv, sizeof(DB_MPREG), NULL, &mpreg)) != 0)
		return (ret);

	mpreg->ftype = ftype;
	mpreg->pgin = pgin;
	mpreg->pgout = pgout;

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	LIST_INSERT_HEAD(&dbmp->dbregq, mpreg, q);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	return (0);
}
