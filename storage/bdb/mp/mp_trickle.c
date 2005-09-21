/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_trickle.c,v 11.35 2004/10/15 16:59:43 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

static int __memp_trickle __P((DB_ENV *, int, int *));

/*
 * __memp_trickle_pp --
 *	DB_ENV->memp_trickle pre/post processing.
 *
 * PUBLIC: int __memp_trickle_pp __P((DB_ENV *, int, int *));
 */
int
__memp_trickle_pp(dbenv, pct, nwrotep)
	DB_ENV *dbenv;
	int pct, *nwrotep;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_trickle", DB_INIT_MPOOL);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_trickle(dbenv, pct, nwrotep);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __memp_trickle --
 *	DB_ENV->memp_trickle.
 */
static int
__memp_trickle(dbenv, pct, nwrotep)
	DB_ENV *dbenv;
	int pct, *nwrotep;
{
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	u_int32_t dirty, i, total, dtmp, wrote;
	int n, ret;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	if (nwrotep != NULL)
		*nwrotep = 0;

	if (pct < 1 || pct > 100)
		return (EINVAL);

	/*
	 * If there are sufficient clean buffers, no buffers or no dirty
	 * buffers, we're done.
	 *
	 * XXX
	 * Using hash_page_dirty is our only choice at the moment, but it's not
	 * as correct as we might like in the presence of pools having more
	 * than one page size, as a free 512B buffer isn't the same as a free
	 * 8KB buffer.
	 *
	 * Loop through the caches counting total/dirty buffers.
	 */
	for (ret = 0, i = dirty = total = 0; i < mp->nreg; ++i) {
		c_mp = dbmp->reginfo[i].primary;
		total += c_mp->stat.st_pages;
		__memp_stat_hash(&dbmp->reginfo[i], c_mp, &dtmp);
		dirty += dtmp;
	}

	/*
	 * !!!
	 * Be careful in modifying this calculation, total may be 0.
	 */
	n = ((total * (u_int)pct) / 100) - (total - dirty);
	if (dirty == 0 || n <= 0)
		return (0);

	ret = __memp_sync_int(
	    dbenv, NULL, (u_int32_t)n, DB_SYNC_TRICKLE, &wrote);
	mp->stat.st_page_trickle += wrote;
	if (nwrotep != NULL)
		*nwrotep = (int)wrote;

	return (ret);
}
