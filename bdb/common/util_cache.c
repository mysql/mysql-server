/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: util_cache.c,v 1.3 2002/04/04 18:50:10 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __db_util_cache --
 *	Compute if we have enough cache.
 *
 * PUBLIC: int __db_util_cache __P((DB_ENV *, DB *, u_int32_t *, int *));
 */
int
__db_util_cache(dbenv, dbp, cachep, resizep)
	DB_ENV *dbenv;
	DB *dbp;
	u_int32_t *cachep;
	int *resizep;
{
	DBTYPE type;
	DB_BTREE_STAT *bsp;
	DB_HASH_STAT *hsp;
	DB_QUEUE_STAT *qsp;
	u_int32_t pgsize;
	int ret;
	void *sp;

	/*
	 * The current cache size is in cachep.  If it's insufficient, set the
	 * the memory referenced by resizep to 1 and set cachep to the minimum
	 * size needed.
	 */
	if ((ret = dbp->get_type(dbp, &type)) != 0) {
		dbenv->err(dbenv, ret, "DB->get_type");
		return (ret);
	}

	if ((ret = dbp->stat(dbp, &sp, DB_FAST_STAT)) != 0) {
		dbenv->err(dbenv, ret, "DB->stat");
		return (ret);
	}

	switch (type) {
	case DB_QUEUE:
		qsp = (DB_QUEUE_STAT *)sp;
		pgsize = qsp->qs_pagesize;
		break;
	case DB_HASH:
		hsp = (DB_HASH_STAT *)sp;
		pgsize = hsp->hash_pagesize;
		break;
	case DB_BTREE:
	case DB_RECNO:
		bsp = (DB_BTREE_STAT *)sp;
		pgsize = bsp->bt_pagesize;
		break;
	default:
		dbenv->err(dbenv, ret, "unknown database type: %d", type);
		return (EINVAL);
	}
	free(sp);

	/*
	 * Make sure our current cache is big enough.  We want at least
	 * DB_MINPAGECACHE pages in the cache.
	 */
	if ((*cachep / pgsize) < DB_MINPAGECACHE) {
		*resizep = 1;
		*cachep = pgsize * DB_MINPAGECACHE;
	} else
		*resizep = 0;

	return (0);
}
