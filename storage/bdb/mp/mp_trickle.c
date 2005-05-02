/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_trickle.c,v 11.24 2002/08/06 06:13:53 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

/*
 * __memp_trickle --
 *	Keep a specified percentage of the buffers clean.
 *
 * PUBLIC: int __memp_trickle __P((DB_ENV *, int, int *));
 */
int
__memp_trickle(dbenv, pct, nwrotep)
	DB_ENV *dbenv;
	int pct, *nwrotep;
{
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	u_int32_t clean, dirty, i, total, dtmp;
	int ret, wrote;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_trickle", DB_INIT_MPOOL);

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

	clean = total - dirty;
	if (clean == total || (clean * 100) / total >= (u_long)pct)
		return (0);

	if (nwrotep == NULL)
		nwrotep = &wrote;
	ret = __memp_sync_int(dbenv, NULL,
	    ((total * pct) / 100) - clean, DB_SYNC_TRICKLE, nwrotep);

	mp->stat.st_page_trickle += *nwrotep;

	return (ret);
}
