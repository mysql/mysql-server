/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fput.c,v 11.16 2000/11/30 00:58:41 ubell Exp $";
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
 * memp_fput --
 *	Mpool file put function.
 */
int
memp_fput(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	int ret, wrote;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fput(dbmfp, pgaddr, flags));
#endif

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	if (flags) {
		if ((ret = __db_fchk(dbenv, "memp_fput", flags,
		    DB_MPOOL_CLEAN | DB_MPOOL_DIRTY | DB_MPOOL_DISCARD)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbenv, "memp_fput",
		    flags, DB_MPOOL_CLEAN, DB_MPOOL_DIRTY)) != 0)
			return (ret);

		if (LF_ISSET(DB_MPOOL_DIRTY) && F_ISSET(dbmfp, MP_READONLY)) {
			__db_err(dbenv,
			    "%s: dirty flag set for readonly file page",
			    __memp_fn(dbmfp));
			return (EACCES);
		}
	}

	R_LOCK(dbenv, dbmp->reginfo);

	/* Decrement the pinned reference count. */
	if (dbmfp->pinref == 0) {
		__db_err(dbenv,
		    "%s: more pages returned than retrieved", __memp_fn(dbmfp));
		R_UNLOCK(dbenv, dbmp->reginfo);
		return (EINVAL);
	} else
		--dbmfp->pinref;

	/*
	 * If we're mapping the file, there's nothing to do.  Because we can
	 * stop mapping the file at any time, we have to check on each buffer
	 * to see if the address we gave the application was part of the map
	 * region.
	 */
	if (dbmfp->addr != NULL && pgaddr >= dbmfp->addr &&
	    (u_int8_t *)pgaddr <= (u_int8_t *)dbmfp->addr + dbmfp->len) {
		R_UNLOCK(dbenv, dbmp->reginfo);
		return (0);
	}

	/* Convert the page address to a buffer header. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));

	/* Convert the buffer header to a cache. */
	c_mp = BH_TO_CACHE(dbmp, bhp);

/* UNLOCK THE REGION, LOCK THE CACHE. */

	/* Set/clear the page bits. */
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

	/*
	 * If the page is dirty and being scheduled to be written as part of
	 * a checkpoint, we no longer know that the log is up-to-date.
	 */
	if (F_ISSET(bhp, BH_DIRTY) && F_ISSET(bhp, BH_SYNC))
		F_SET(bhp, BH_SYNC_LOGFLSH);

	/*
	 * Check for a reference count going to zero.  This can happen if the
	 * application returns a page twice.
	 */
	if (bhp->ref == 0) {
		__db_err(dbenv, "%s: page %lu: unpinned page returned",
		    __memp_fn(dbmfp), (u_long)bhp->pgno);
		R_UNLOCK(dbenv, dbmp->reginfo);
		return (EINVAL);
	}

	/*
	 * If more than one reference to the page, we're done.  Ignore the
	 * discard flags (for now) and leave it at its position in the LRU
	 * chain.  The rest gets done at last reference close.
	 */
	if (--bhp->ref > 0) {
		R_UNLOCK(dbenv, dbmp->reginfo);
		return (0);
	}

	/*
	 * Move the buffer to the head/tail of the LRU chain.  We do this
	 * before writing the buffer for checkpoint purposes, as the write
	 * can discard the region lock and allow another process to acquire
	 * buffer.  We could keep that from happening, but there seems no
	 * reason to do so.
	 */
	SH_TAILQ_REMOVE(&c_mp->bhq, bhp, q, __bh);
	if (F_ISSET(bhp, BH_DISCARD))
		SH_TAILQ_INSERT_HEAD(&c_mp->bhq, bhp, q, __bh);
	else
		SH_TAILQ_INSERT_TAIL(&c_mp->bhq, bhp, q);

	/*
	 * If this buffer is scheduled for writing because of a checkpoint, we
	 * need to write it (if it's dirty), or update the checkpoint counters
	 * (if it's not dirty).  If we try to write it and can't, that's not
	 * necessarily an error as it's not completely unreasonable that the
	 * application have permission to write the underlying file, but set a
	 * flag so that the next time the memp_sync function is called we try
	 * writing it there, as the checkpoint thread of control better be able
	 * to write all of the files.
	 */
	if (F_ISSET(bhp, BH_SYNC)) {
		if (F_ISSET(bhp, BH_DIRTY)) {
			if (__memp_bhwrite(dbmp,
			    dbmfp->mfp, bhp, NULL, &wrote) != 0 || !wrote)
				F_SET(mp, MP_LSN_RETRY);
		} else {
			F_CLR(bhp, BH_SYNC);

			--mp->lsn_cnt;
			--dbmfp->mfp->lsn_cnt;
		}
	}

	R_UNLOCK(dbenv, dbmp->reginfo);
	return (0);
}
