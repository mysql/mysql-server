/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: hash_meta.c,v 11.10 2000/12/21 21:54:35 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "hash.h"
#include "db_shash.h"
#include "lock.h"
#include "txn.h"

/*
 * Acquire the meta-data page.
 *
 * PUBLIC: int __ham_get_meta __P((DBC *));
 */
int
__ham_get_meta(dbc)
	DBC *dbc;
{
	HASH_CURSOR *hcp;
	HASH *hashp;
	DB *dbp;
	int ret;

	hcp = (HASH_CURSOR *)dbc->internal;
	dbp = dbc->dbp;
	hashp = dbp->h_internal;

	if (dbp->dbenv != NULL &&
	    STD_LOCKING(dbc) && !F_ISSET(dbc, DBC_RECOVER)) {
		dbc->lock.pgno = hashp->meta_pgno;
		if ((ret = lock_get(dbp->dbenv, dbc->locker,
		    DB_NONBLOCK(dbc) ? DB_LOCK_NOWAIT : 0,
		    &dbc->lock_dbt, DB_LOCK_READ, &hcp->hlock)) != 0)
			return (ret);
	}

	if ((ret = memp_fget(dbc->dbp->mpf,
	    &hashp->meta_pgno, DB_MPOOL_CREATE, &(hcp->hdr))) != 0 &&
	    hcp->hlock.off != LOCK_INVALID) {
		(void)lock_put(dbc->dbp->dbenv, &hcp->hlock);
		hcp->hlock.off = LOCK_INVALID;
	}

	return (ret);
}

/*
 * Release the meta-data page.
 *
 * PUBLIC: int __ham_release_meta __P((DBC *));
 */
int
__ham_release_meta(dbc)
	DBC *dbc;
{
	HASH_CURSOR *hcp;

	hcp = (HASH_CURSOR *)dbc->internal;

	if (hcp->hdr)
		(void)memp_fput(dbc->dbp->mpf, hcp->hdr,
		    F_ISSET(hcp, H_DIRTY) ? DB_MPOOL_DIRTY : 0);
	hcp->hdr = NULL;
	if (!F_ISSET(dbc, DBC_RECOVER) &&
	    dbc->txn == NULL && hcp->hlock.off != LOCK_INVALID)
		(void)lock_put(dbc->dbp->dbenv, &hcp->hlock);
	hcp->hlock.off = LOCK_INVALID;
	F_CLR(hcp, H_DIRTY);

	return (0);
}

/*
 * Mark the meta-data page dirty.
 *
 * PUBLIC: int __ham_dirty_meta __P((DBC *));
 */
int
__ham_dirty_meta(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_LOCK _tmp;
	HASH *hashp;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	hashp = dbp->h_internal;
	hcp = (HASH_CURSOR *)dbc->internal;

	ret = 0;
	if (STD_LOCKING(dbc) && !F_ISSET(dbc, DBC_RECOVER)) {
		dbc->lock.pgno = hashp->meta_pgno;
		if ((ret = lock_get(dbp->dbenv, dbc->locker,
		    DB_NONBLOCK(dbc) ? DB_LOCK_NOWAIT : 0,
		    &dbc->lock_dbt, DB_LOCK_WRITE, &_tmp)) == 0) {
			ret = lock_put(dbp->dbenv, &hcp->hlock);
			hcp->hlock = _tmp;
		}
	}

	if (ret == 0)
		F_SET(hcp, H_DIRTY);
	return (ret);
}
