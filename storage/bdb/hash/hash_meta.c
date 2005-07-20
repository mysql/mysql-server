/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: hash_meta.c,v 11.31 2004/09/22 03:46:22 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"

/*
 * Acquire the meta-data page.
 *
 * PUBLIC: int __ham_get_meta __P((DBC *));
 */
int
__ham_get_meta(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	HASH *hashp;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hashp = dbp->h_internal;
	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __db_lget(dbc, 0,
	     hashp->meta_pgno, DB_LOCK_READ, 0, &hcp->hlock)) != 0)
		return (ret);

	if ((ret = __memp_fget(mpf,
	    &hashp->meta_pgno, DB_MPOOL_CREATE, &(hcp->hdr))) != 0)
		(void)__LPUT(dbc, hcp->hlock);

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
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;

	mpf = dbc->dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (hcp->hdr)
		(void)__memp_fput(mpf, hcp->hdr,
		    F_ISSET(hcp, H_DIRTY) ? DB_MPOOL_DIRTY : 0);
	hcp->hdr = NULL;
	F_CLR(hcp, H_DIRTY);

	return (__TLPUT(dbc, hcp->hlock));
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
	HASH *hashp;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	hashp = dbp->h_internal;
	hcp = (HASH_CURSOR *)dbc->internal;

	ret = 0;

	ret = __db_lget(dbc, LCK_COUPLE,
	     hashp->meta_pgno, DB_LOCK_WRITE, 0, &hcp->hlock);

	if (ret == 0)
		F_SET(hcp, H_DIRTY);
	return (ret);
}
