/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fset.c,v 11.13 2000/11/30 00:58:41 ubell Exp $";
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
 * memp_fset --
 *	Mpool page set-flag routine.
 */
int
memp_fset(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	int ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fset(dbmfp, pgaddr, flags));
#endif

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	if (flags == 0)
		return (__db_ferr(dbenv, "memp_fset", 1));

	if ((ret = __db_fchk(dbenv, "memp_fset", flags,
	    DB_MPOOL_DIRTY | DB_MPOOL_CLEAN | DB_MPOOL_DISCARD)) != 0)
		return (ret);
	if ((ret = __db_fcchk(dbenv, "memp_fset",
	    flags, DB_MPOOL_CLEAN, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	if (LF_ISSET(DB_MPOOL_DIRTY) && F_ISSET(dbmfp, MP_READONLY)) {
		__db_err(dbenv, "%s: dirty flag set for readonly file page",
		    __memp_fn(dbmfp));
		return (EACCES);
	}

	/* Convert the page address to a buffer header. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));

	/* Convert the buffer header to a cache. */
	c_mp = BH_TO_CACHE(dbmp, bhp);

	R_LOCK(dbenv, dbmp->reginfo);

	if (LF_ISSET(DB_MPOOL_CLEAN) && F_ISSET(bhp, BH_DIRTY)) {
		++c_mp->stat.st_page_clean;
		--c_mp->stat.st_page_dirty;
		F_CLR(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DIRTY) && !F_ISSET(bhp, BH_DIRTY)) {
		--c_mp->stat.st_page_clean;
		++c_mp->stat.st_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DISCARD))
		F_SET(bhp, BH_DISCARD);

	R_UNLOCK(dbenv, dbmp->reginfo);
	return (0);
}
