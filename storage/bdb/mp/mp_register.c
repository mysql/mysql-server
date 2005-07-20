/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_register.c,v 11.26 2004/07/15 15:52:54 sue Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

/*
 * memp_register_pp --
 *	DB_ENV->memp_register pre/post processing.
 *
 * PUBLIC: int __memp_register_pp __P((DB_ENV *, int,
 * PUBLIC:     int (*)(DB_ENV *, db_pgno_t, void *, DBT *),
 * PUBLIC:     int (*)(DB_ENV *, db_pgno_t, void *, DBT *)));
 */
int
__memp_register_pp(dbenv, ftype, pgin, pgout)
	DB_ENV *dbenv;
	int ftype;
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "DB_ENV->memp_register", DB_INIT_MPOOL);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_register(dbenv, ftype, pgin, pgout);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * memp_register --
 *	DB_ENV->memp_register.
 *
 * PUBLIC: int __memp_register __P((DB_ENV *, int,
 * PUBLIC:     int (*)(DB_ENV *, db_pgno_t, void *, DBT *),
 * PUBLIC:     int (*)(DB_ENV *, db_pgno_t, void *, DBT *)));
 */
int
__memp_register(dbenv, ftype, pgin, pgout)
	DB_ENV *dbenv;
	int ftype;
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
{
	DB_MPOOL *dbmp;
	DB_MPREG *mpreg;
	int ret;

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
	if ((ret = __os_malloc(dbenv, sizeof(DB_MPREG), &mpreg)) != 0)
		return (ret);

	mpreg->ftype = ftype;
	mpreg->pgin = pgin;
	mpreg->pgout = pgout;

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	LIST_INSERT_HEAD(&dbmp->dbregq, mpreg, q);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	return (0);
}
