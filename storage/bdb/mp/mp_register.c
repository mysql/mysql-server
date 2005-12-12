/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_register.c,v 12.6 2005/10/07 20:21:33 ubell Exp $
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
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "DB_ENV->memp_register", DB_INIT_MPOOL);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv,
	    (__memp_register(dbenv, ftype, pgin, pgout)), ret);
	ENV_LEAVE(dbenv, ip);
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
	 * We keep the DB pgin/pgout functions outside of the linked list
	 * to avoid locking/unlocking the linked list on every page I/O.
	 *
	 * The Berkeley DB I/O conversion functions are registered when the
	 * environment is first created, so there's no need for locking here.
	 */
	if (ftype == DB_FTYPE_SET) {
		if (dbmp->pg_inout != NULL)
			return (0);
		if ((ret =
		    __os_malloc(dbenv, sizeof(DB_MPREG), &dbmp->pg_inout)) != 0)
			return (ret);
		dbmp->pg_inout->ftype = ftype;
		dbmp->pg_inout->pgin = pgin;
		dbmp->pg_inout->pgout = pgout;
		return (0);
	}

	/*
	 * The item may already have been registered.  If already registered,
	 * just update the entry, although it's probably unchanged.
	 */
	MUTEX_LOCK(dbenv, dbmp->mutex);
	for (mpreg = LIST_FIRST(&dbmp->dbregq);
	    mpreg != NULL; mpreg = LIST_NEXT(mpreg, q))
		if (mpreg->ftype == ftype) {
			mpreg->pgin = pgin;
			mpreg->pgout = pgout;
			break;
		}

	if (mpreg == NULL) {			/* New entry. */
		if ((ret = __os_malloc(dbenv, sizeof(DB_MPREG), &mpreg)) != 0)
			return (ret);
		mpreg->ftype = ftype;
		mpreg->pgin = pgin;
		mpreg->pgout = pgout;

		LIST_INSERT_HEAD(&dbmp->dbregq, mpreg, q);
	}
	MUTEX_UNLOCK(dbenv, dbmp->mutex);

	return (0);
}
