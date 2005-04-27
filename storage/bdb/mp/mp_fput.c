/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fput.c,v 11.36 2002/08/09 19:04:11 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

/*
 * __memp_fput --
 *	Mpool file put function.
 *
 * PUBLIC: int __memp_fput __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__memp_fput(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *argbhp, *bhp, *prev;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp;
	u_int32_t n_cache;
	int adjust, ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

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

	/*
	 * If we're mapping the file, there's nothing to do.  Because we can
	 * stop mapping the file at any time, we have to check on each buffer
	 * to see if the address we gave the application was part of the map
	 * region.
	 */
	if (dbmfp->addr != NULL && pgaddr >= dbmfp->addr &&
	    (u_int8_t *)pgaddr <= (u_int8_t *)dbmfp->addr + dbmfp->len)
		return (0);

#ifdef DIAGNOSTIC
	/*
	 * Decrement the per-file pinned buffer count (mapped pages aren't
	 * counted).
	 */
	R_LOCK(dbenv, dbmp->reginfo);
	if (dbmfp->pinref == 0) {
		ret = EINVAL;
		__db_err(dbenv,
		    "%s: more pages returned than retrieved", __memp_fn(dbmfp));
	} else {
		ret = 0;
		--dbmfp->pinref;
	}
	R_UNLOCK(dbenv, dbmp->reginfo);
	if (ret != 0)
		return (ret);
#endif

	/* Convert a page address to a buffer header and hash bucket. */
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

	/*
	 * Check for a reference count going to zero.  This can happen if the
	 * application returns a page twice.
	 */
	if (bhp->ref == 0) {
		__db_err(dbenv, "%s: page %lu: unpinned page returned",
		    __memp_fn(dbmfp), (u_long)bhp->pgno);
		MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
		return (EINVAL);
	}

	/*
	 * If more than one reference to the page or a reference other than a
	 * thread waiting to flush the buffer to disk, we're done.  Ignore the
	 * discard flags (for now) and leave the buffer's priority alone.
	 */
	if (--bhp->ref > 1 || (bhp->ref == 1 && !F_ISSET(bhp, BH_LOCKED))) {
		MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
		return (0);
	}

	/* Update priority values. */
	if (F_ISSET(bhp, BH_DISCARD) ||
	    dbmfp->mfp->priority == MPOOL_PRI_VERY_LOW)
		bhp->priority = 0;
	else {
		/*
		 * We don't lock the LRU counter or the stat.st_pages field, if
		 * we get garbage (which won't happen on a 32-bit machine), it
		 * only means a buffer has the wrong priority.
		 */
		bhp->priority = c_mp->lru_count;

		adjust = 0;
		if (dbmfp->mfp->priority != 0)
			adjust =
			    (int)c_mp->stat.st_pages / dbmfp->mfp->priority;
		if (F_ISSET(bhp, BH_DIRTY))
			adjust += c_mp->stat.st_pages / MPOOL_PRI_DIRTY;

		if (adjust > 0) {
			if (UINT32_T_MAX - bhp->priority <= (u_int32_t)adjust)
				bhp->priority += adjust;
		} else if (adjust < 0)
			if (bhp->priority > (u_int32_t)-adjust)
				bhp->priority += adjust;
	}

	/*
	 * Buffers on hash buckets are sorted by priority -- move the buffer
	 * to the correct position in the list.
	 */
	argbhp = bhp;
	SH_TAILQ_REMOVE(&hp->hash_bucket, argbhp, hq, __bh);

	prev = NULL;
	for (bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
	    bhp != NULL; prev = bhp, bhp = SH_TAILQ_NEXT(bhp, hq, __bh))
		if (bhp->priority > argbhp->priority)
			break;
	if (prev == NULL)
		SH_TAILQ_INSERT_HEAD(&hp->hash_bucket, argbhp, hq, __bh);
	else
		SH_TAILQ_INSERT_AFTER(&hp->hash_bucket, prev, argbhp, hq, __bh);

	/* Reset the hash bucket's priority. */
	hp->hash_priority = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)->priority;

#ifdef DIAGNOSTIC
	__memp_check_order(hp);
#endif

	/*
	 * The sync code has a separate counter for buffers on which it waits.
	 * It reads that value without holding a lock so we update it as the
	 * last thing we do.  Once that value goes to 0, we won't see another
	 * reference to that buffer being returned to the cache until the sync
	 * code has finished, so we're safe as long as we don't let the value
	 * go to 0 before we finish with the buffer.
	 */
	if (F_ISSET(argbhp, BH_LOCKED) && argbhp->ref_sync != 0)
		--argbhp->ref_sync;

	MUTEX_UNLOCK(dbenv, &hp->hash_mutex);

	return (0);
}
