/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_fset.c,v 11.34 2004/10/15 16:59:43 bostic Exp $
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
 * __memp_fset_pp --
 *	DB_MPOOLFILE->set pre/post processing.
 *
 * PUBLIC: int __memp_fset_pp __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__memp_fset_pp(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int rep_check, ret;

	dbenv = dbmfp->dbenv;

	PANIC_CHECK(dbenv);
	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, "DB_MPOOLFILE->set");

	/* Validate arguments. */
	if (flags == 0)
		return (__db_ferr(dbenv, "memp_fset", 1));

	if ((ret = __db_fchk(dbenv, "memp_fset", flags,
	    DB_MPOOL_CLEAN | DB_MPOOL_DIRTY | DB_MPOOL_DISCARD)) != 0)
		return (ret);
	if ((ret = __db_fcchk(dbenv, "memp_fset",
	    flags, DB_MPOOL_CLEAN, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	if (LF_ISSET(DB_MPOOL_DIRTY) && F_ISSET(dbmfp, MP_READONLY)) {
		__db_err(dbenv, "%s: dirty flag set for readonly file page",
		    __memp_fn(dbmfp));
		return (EACCES);
	}

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_fset(dbmfp, pgaddr, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __memp_fset --
 *	DB_MPOOLFILE->set.
 *
 * PUBLIC: int __memp_fset __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__memp_fset(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp;
	u_int32_t n_cache;

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;

	/* Convert the page address to a buffer header and hash bucket. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));
	n_cache = NCACHE(dbmp->reginfo[0].primary, bhp->mf_offset, bhp->pgno);
	c_mp = dbmp->reginfo[n_cache].primary;
	hp = R_ADDR(&dbmp->reginfo[n_cache], c_mp->htab);
	hp = &hp[NBUCKET(c_mp, bhp->mf_offset, bhp->pgno)];

	MUTEX_LOCK(dbenv, &hp->hash_mutex);

	/* Set/clear the page bits. */
	if (LF_ISSET(DB_MPOOL_CLEAN) &&
	    F_ISSET(bhp, BH_DIRTY) && !F_ISSET(bhp, BH_DIRTY_CREATE)) {
		DB_ASSERT(hp->hash_page_dirty != 0);
		--hp->hash_page_dirty;
		F_CLR(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DIRTY) && !F_ISSET(bhp, BH_DIRTY)) {
		++hp->hash_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DISCARD))
		F_SET(bhp, BH_DISCARD);

	MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
	return (0);
}
