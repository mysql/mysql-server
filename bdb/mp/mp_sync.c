/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_sync.c,v 11.29 2001/01/11 18:19:53 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
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

static int __bhcmp __P((const void *, const void *));
static int __memp_fsync __P((DB_MPOOLFILE *));
static int __memp_sballoc __P((DB_ENV *, BH ***, u_int32_t *));

/*
 * memp_sync --
 *	Mpool sync function.
 */
int
memp_sync(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	BH *bhp, **bharray;
	DB_MPOOL *dbmp;
	DB_LSN tlsn;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	u_int32_t ar_cnt, i, ndirty;
	int ret, retry_done, retry_need, wrote;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_sync(dbenv, lsnp));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->mp_handle, DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/*
	 * If no LSN is provided, flush the entire cache.
	 *
	 * !!!
	 * Our current behavior is to flush the entire cache, so there's
	 * nothing special we have to do here other than deal with NULL
	 * pointers.
	 */
	if (lsnp == NULL) {
		ZERO_LSN(tlsn);
		lsnp = &tlsn;
		F_SET(mp, MP_LSN_RETRY);
	} else if (!LOGGING_ON(dbenv)) {
		__db_err(dbenv, "memp_sync: requires logging");
		return (EINVAL);
	}

	/*
	 * Sync calls are single-threaded so that we don't have multiple
	 * threads, with different checkpoint LSNs, walking the caches
	 * and updating the checkpoint LSNs and how many buffers remain
	 * to be written for the checkpoint.  This shouldn't be a problem,
	 * any application that has multiple checkpoint threads isn't what
	 * I'd call trustworthy.
	 */
	MUTEX_LOCK(dbenv, &mp->sync_mutex, dbenv->lockfhp);

	/*
	 * If the application is asking about a previous call to memp_sync(),
	 * and we haven't found any buffers that the application holding the
	 * pin couldn't write, return yes or no based on the current count.
	 * Note, if the application is asking about a LSN *smaller* than one
	 * we've already handled or are currently handling, then we return a
	 * result based on the count for the larger LSN.
	 */
	R_LOCK(dbenv, dbmp->reginfo);
	if (!IS_ZERO_LSN(*lsnp) &&
	    !F_ISSET(mp, MP_LSN_RETRY) && log_compare(lsnp, &mp->lsn) <= 0) {
		if (mp->lsn_cnt == 0) {
			*lsnp = mp->lsn;
			ret = 0;
		} else
			ret = DB_INCOMPLETE;

		R_UNLOCK(dbenv, dbmp->reginfo);
		MUTEX_UNLOCK(dbenv, &mp->sync_mutex);
		return (ret);
	}

	/*
	 * Allocate room for a list of buffers, and decide how many buffers
	 * we can pin down.
	 *
	 * !!!
	 * Note: __memp_sballoc has released the region lock if we're not
	 * continuing forward.
	 */
	if ((ret =
	    __memp_sballoc(dbenv, &bharray, &ndirty)) != 0 || ndirty == 0) {
		MUTEX_UNLOCK(dbenv, &mp->sync_mutex);
		return (ret);
	}

	retry_done = 0;
retry:	retry_need = 0;
	/*
	 * Start a new checkpoint.
	 *
	 * Save the LSN.  We know that it's a new LSN, a retry, or larger than
	 * the one for which we were already doing a checkpoint.  (BTW, I don't
	 * expect to see multiple LSN's from the same or multiple processes,
	 * but You Just Never Know.  Responding as if they all called with the
	 * largest of the LSNs specified makes everything work.)
	 *
	 * We don't currently use the LSN we save.  We could potentially save
	 * the last-written LSN in each buffer header and use it to determine
	 * what buffers need to be written.  The problem with this is that it's
	 * sizeof(LSN) more bytes of buffer header.  We currently write all the
	 * dirty buffers instead, but with a sufficiently large cache that's
	 * going to be a problem.
	 */
	mp->lsn = *lsnp;

	/*
	 * Clear the global count of buffers waiting to be written, walk the
	 * list of files clearing the count of buffers waiting to be written.
	 *
	 * Clear the retry flag.
	 */
	mp->lsn_cnt = 0;
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile))
		mfp->lsn_cnt = 0;
	F_CLR(mp, MP_LSN_RETRY);

	/*
	 * Walk each cache's list of buffers and mark all dirty buffers to be
	 * written and all pinned buffers to be potentially written (we can't
	 * know if they'll need to be written until the holder returns them to
	 * the cache).  We do this in one pass while holding the region locked
	 * so that processes can't make new buffers dirty, causing us to never
	 * finish.  Since the application may have restarted the sync using a
	 * different LSN value, clear any BH_SYNC | BH_SYNC_LOGFLSH flags that
	 * appear leftover from previous calls.
	 *
	 * Keep a count of the total number of buffers we need to write in
	 * MPOOL->lsn_cnt, and for each file, in MPOOLFILE->lsn_count.
	 */
	for (ar_cnt = 0, i = 0; i < mp->nreg; ++i) {
		c_mp = dbmp->reginfo[i].primary;
		for (bhp = SH_TAILQ_FIRST(&c_mp->bhq, __bh);
		    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh)) {
			if (F_ISSET(bhp, BH_DIRTY) || bhp->ref != 0) {
				F_SET(bhp, BH_SYNC);

				++mp->lsn_cnt;

				mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
				++mfp->lsn_cnt;

				/*
				 * If the buffer isn't being used, we can write
				 * it immediately, so increment its reference
				 * count to lock it down, and save a reference
				 * to it.
				 *
				 * If we've run out space to store buffer refs,
				 * we're screwed.  We don't want to realloc the
				 * array while holding a region lock, so we set
				 * a flag and deal with it later.
				 */
				if (bhp->ref == 0) {
					++bhp->ref;
					bharray[ar_cnt] = bhp;

					if (++ar_cnt >= ndirty) {
						retry_need = 1;
						break;
					}
				}
			} else
				if (F_ISSET(bhp, BH_SYNC))
					F_CLR(bhp, BH_SYNC | BH_SYNC_LOGFLSH);
		}
		if (ar_cnt >= ndirty)
			break;
	}

	/* If there no buffers we can write immediately, we're done. */
	if (ar_cnt == 0) {
		ret = mp->lsn_cnt ? DB_INCOMPLETE : 0;
		goto done;
	}

	R_UNLOCK(dbenv, dbmp->reginfo);

	/*
	 * Sort the buffers we're going to write immediately.
	 *
	 * We try and write the buffers in file/page order: it should reduce
	 * seeks by the underlying filesystem and possibly reduce the actual
	 * number of writes.
	 */
	if (ar_cnt > 1)
		qsort(bharray, ar_cnt, sizeof(BH *), __bhcmp);

	/*
	 * Flush the log.  We have to ensure the log records reflecting the
	 * changes on the database pages we're writing have already made it
	 * to disk.  We usually do that as we write each page, but if we
	 * are going to write a large number of pages, repeatedly acquiring
	 * the log region lock is going to be expensive.  Flush the entire
	 * log now, so that sync doesn't require any more log flushes.
	 */
	if (LOGGING_ON(dbenv) && (ret = log_flush(dbenv, NULL)) != 0)
		goto done;

	R_LOCK(dbenv, dbmp->reginfo);

	/* Walk the array, writing buffers. */
	for (i = 0; i < ar_cnt; ++i) {
		/*
		 * It's possible for a thread to have gotten the buffer since
		 * we listed it for writing.  If the reference count is still
		 * 1, we're the only ones using the buffer, go ahead and write.
		 * If it's >1, then skip the buffer and assume that it will be
		 * written when it's returned to the cache.
		 */
		if (bharray[i]->ref > 1) {
			--bharray[i]->ref;
			continue;
		}

		/* Write the buffer. */
		mfp = R_ADDR(dbmp->reginfo, bharray[i]->mf_offset);
		ret = __memp_bhwrite(dbmp, mfp, bharray[i], NULL, &wrote);

		/* Release the buffer. */
		--bharray[i]->ref;

		if (ret == 0 && wrote)
			continue;

		/*
		 * Any process syncing the shared memory buffer pool had best
		 * be able to write to any underlying file. Be understanding,
		 * but firm, on this point.
		 */
		if (ret == 0) {
			__db_err(dbenv, "%s: unable to flush page: %lu",
			    __memp_fns(dbmp, mfp), (u_long)bharray[i]->pgno);
			ret = EPERM;
		}

		/*
		 * On error, clear MPOOL->lsn and set MP_LSN_RETRY so that no
		 * future checkpoint return can depend on this failure.  Clear
		 * the buffer's BH_SYNC flag, because it's used to determine
		 * if lsn_cnt values are incremented/decremented.  Don't bother
		 * to reset/clear:
		 *
		 *	MPOOL->lsn_cnt
		 *	MPOOLFILE->lsn_cnt
		 *
		 * they don't make any difference.
		 */
		ZERO_LSN(mp->lsn);
		F_SET(mp, MP_LSN_RETRY);

		/* Release any buffers we're still pinning down. */
		while (++i < ar_cnt) {
			bhp = bharray[i];
			--bhp->ref;
			F_CLR(bhp, BH_SYNC | BH_SYNC_LOGFLSH);
		}

		goto done;
	}

	ret = mp->lsn_cnt != 0 ? DB_INCOMPLETE : 0;

	/*
	 * If there were too many buffers and we're not returning an error, we
	 * re-try the checkpoint once -- since we allocated 80% of the total
	 * buffer count, once should be enough. If it still doesn't work, some
	 * other thread of control is dirtying buffers as fast as we're writing
	 * them, and we might as well give up for now.  In the latter case, set
	 * the global retry flag, we'll have to start from scratch on the next
	 * checkpoint.
	 */
	if (retry_need) {
		if (retry_done) {
			ret = DB_INCOMPLETE;
			F_SET(mp, MP_LSN_RETRY);
		} else {
			retry_done = 1;
			goto retry;
		}
	}

done:	R_UNLOCK(dbenv, dbmp->reginfo);
	MUTEX_UNLOCK(dbenv, &mp->sync_mutex);

	__os_free(bharray, ndirty * sizeof(BH *));

	return (ret);
}

/*
 * memp_fsync --
 *	Mpool file sync function.
 */
int
memp_fsync(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int is_tmp;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fsync(dbmfp));
#endif

	PANIC_CHECK(dbenv);

	/*
	 * If this handle doesn't have a file descriptor that's open for
	 * writing, or if the file is a temporary, there's no reason to
	 * proceed further.
	 */
	if (F_ISSET(dbmfp, MP_READONLY))
		return (0);

	R_LOCK(dbenv, dbmp->reginfo);
	is_tmp = F_ISSET(dbmfp->mfp, MP_TEMP);
	R_UNLOCK(dbenv, dbmp->reginfo);
	if (is_tmp)
		return (0);

	return (__memp_fsync(dbmfp));
}

/*
 * __mp_xxx_fh --
 *	Return a file descriptor for DB 1.85 compatibility locking.
 *
 * PUBLIC: int __mp_xxx_fh __P((DB_MPOOLFILE *, DB_FH **));
 */
int
__mp_xxx_fh(dbmfp, fhp)
	DB_MPOOLFILE *dbmfp;
	DB_FH **fhp;
{
	/*
	 * This is a truly spectacular layering violation, intended ONLY to
	 * support compatibility for the DB 1.85 DB->fd call.
	 *
	 * Sync the database file to disk, creating the file as necessary.
	 *
	 * We skip the MP_READONLY and MP_TEMP tests done by memp_fsync(3).
	 * The MP_READONLY test isn't interesting because we will either
	 * already have a file descriptor (we opened the database file for
	 * reading) or we aren't readonly (we created the database which
	 * requires write privileges).  The MP_TEMP test isn't interesting
	 * because we want to write to the backing file regardless so that
	 * we get a file descriptor to return.
	 */
	*fhp = &dbmfp->fh;
	return (F_ISSET(&dbmfp->fh, DB_FH_VALID) ? 0 : __memp_fsync(dbmfp));
}

/*
 * __memp_fsync --
 *	Mpool file internal sync function.
 */
static int
__memp_fsync(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	BH *bhp, **bharray;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	size_t mf_offset;
	u_int32_t ar_cnt, i, ndirty;
	int incomplete, ret, retry_done, retry_need, wrote;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;

	R_LOCK(dbenv, dbmp->reginfo);

	/*
	 * Allocate room for a list of buffers, and decide how many buffers
	 * we can pin down.
	 *
	 * !!!
	 * Note: __memp_sballoc has released our region lock if we're not
	 * continuing forward.
	 */
	if ((ret =
	    __memp_sballoc(dbenv, &bharray, &ndirty)) != 0 || ndirty == 0)
		return (ret);

	retry_done = 0;
retry:	retry_need = 0;
	/*
	 * Walk each cache's list of buffers and mark all dirty buffers to be
	 * written and all pinned buffers to be potentially written (we can't
	 * know if they'll need to be written until the holder returns them to
	 * the cache).  We do this in one pass while holding the region locked
	 * so that processes can't make new buffers dirty, causing us to never
	 * finish.
	 */
	mf_offset = R_OFFSET(dbmp->reginfo, dbmfp->mfp);
	for (ar_cnt = 0, incomplete = 0, i = 0; i < mp->nreg; ++i) {
		c_mp = dbmp->reginfo[i].primary;
		for (bhp = SH_TAILQ_FIRST(&c_mp->bhq, __bh);
		    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh)) {
			if (!F_ISSET(bhp, BH_DIRTY) ||
			    bhp->mf_offset != mf_offset)
				continue;
			if (bhp->ref != 0 || F_ISSET(bhp, BH_LOCKED)) {
				incomplete = 1;
				continue;
			}

			/*
			 * If the buffer isn't being used, we can write
			 * it immediately, so increment its reference
			 * count to lock it down, and save a reference
			 * to it.
			 *
			 * If we've run out space to store buffer refs,
			 * we're screwed.  We don't want to realloc the
			 * array while holding a region lock, so we set
			 * a flag and deal with it later.
			 */
			++bhp->ref;
			bharray[ar_cnt] = bhp;
			if (++ar_cnt >= ndirty) {
				retry_need = 1;
				break;
			}
		}
		if (ar_cnt >= ndirty)
			break;
	}

	/* If there no buffers we can write immediately, we're done. */
	if (ar_cnt == 0) {
		ret = 0;
		goto done;
	}

	R_UNLOCK(dbenv, dbmp->reginfo);

	/* Sort the buffers we're going to write. */
	if (ar_cnt > 1)
		qsort(bharray, ar_cnt, sizeof(BH *), __bhcmp);

	R_LOCK(dbenv, dbmp->reginfo);

	/* Walk the array, writing buffers. */
	for (i = 0; i < ar_cnt;) {
		/*
		 * It's possible for a thread to have gotten the buffer since
		 * we listed it for writing.  If the reference count is still
		 * 1, we're the only ones using the buffer, go ahead and write.
		 * If it's >1, then skip the buffer and assume that it will be
		 * written when it's returned to the cache.
		 */
		if (bharray[i]->ref > 1) {
			incomplete = 1;
			--bharray[i++]->ref;
			continue;
		}

		/* Write the buffer. */
		ret = __memp_pgwrite(dbmp, dbmfp, bharray[i], NULL, &wrote);

		/* Release the buffer. */
		--bharray[i++]->ref;

		if (ret == 0) {
			if (!wrote)
				incomplete = 1;
			continue;
		}

		/*
		 * On error:
		 *
		 * Release any buffers we're still pinning down.
		 */
		while (i < ar_cnt)
			--bharray[i++]->ref;
		break;
	}

	/*
	 * If there were too many buffers and we're not returning an error, we
	 * re-try the flush once -- since we allocated 80% of the total
	 * buffer count, once should be enough. If it still doesn't work, some
	 * other thread of control is dirtying buffers as fast as we're writing
	 * them, and we might as well give up.
	 */
	if (retry_need) {
		if (retry_done)
			incomplete = 1;
		else {
			retry_done = 1;
			goto retry;
		}
	}

done:	R_UNLOCK(dbenv, dbmp->reginfo);

	__os_free(bharray, ndirty * sizeof(BH *));

	/*
	 * Sync the underlying file as the last thing we do, so that the OS
	 * has a maximal opportunity to flush buffers before we request it.
	 *
	 * !!!:
	 * Don't lock the region around the sync, fsync(2) has no atomicity
	 * issues.
	 */
	if (ret == 0)
		ret = incomplete ?
		    DB_INCOMPLETE : __os_fsync(dbenv, &dbmfp->fh);

	return (ret);
}

/*
 * __memp_sballoc --
 *	Allocate room for a list of buffers.
 */
static int
__memp_sballoc(dbenv, bharrayp, ndirtyp)
	DB_ENV *dbenv;
	BH ***bharrayp;
	u_int32_t *ndirtyp;
{
	DB_MPOOL *dbmp;
	MPOOL *c_mp, *mp;
	u_int32_t i, nclean, ndirty, maxpin;
	int ret;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/*
	 * We don't want to hold the region lock while we write the buffers,
	 * so only lock it while we create a list.
	 *
	 * Walk through the list of caches, figuring out how many buffers
	 * we're going to need.
	 *
	 * Make a point of not holding the region lock across the library
	 * allocation call.
	 */
	for (nclean = ndirty = 0, i = 0; i < mp->nreg; ++i) {
		c_mp = dbmp->reginfo[i].primary;
		ndirty += c_mp->stat.st_page_dirty;
		nclean += c_mp->stat.st_page_clean;
	}
	R_UNLOCK(dbenv, dbmp->reginfo);
	if (ndirty == 0) {
		*ndirtyp = 0;
		return (0);
	}

	/*
	 * We don't want to pin down the entire buffer cache, otherwise we'll
	 * starve threads needing new pages.  Don't pin down more than 80% of
	 * the cache, making sure that we don't screw up just because only a
	 * few pages have been created.
	 */
	maxpin = ((ndirty + nclean) * 8) / 10;
	if (maxpin < 10)
		maxpin = 10;

	/*
	 * Get a good-sized block of memory to hold buffer pointers, we don't
	 * want to run out, but correct if we want to allocate more than we
	 * would be allowed to store, regardless.
	 */
	ndirty += ndirty / 2 + 10;
	if (ndirty > maxpin)
		ndirty = maxpin;
	if ((ret =
	    __os_malloc(dbenv, ndirty * sizeof(BH *), NULL, bharrayp)) != 0)
		return (ret);

	*ndirtyp = ndirty;

	R_LOCK(dbenv, dbmp->reginfo);

	return (0);
}

static int
__bhcmp(p1, p2)
	const void *p1, *p2;
{
	BH *bhp1, *bhp2;

	bhp1 = *(BH * const *)p1;
	bhp2 = *(BH * const *)p2;

	/* Sort by file (shared memory pool offset). */
	if (bhp1->mf_offset < bhp2->mf_offset)
		return (-1);
	if (bhp1->mf_offset > bhp2->mf_offset)
		return (1);

	/*
	 * !!!
	 * Defend against badly written quicksort code calling the comparison
	 * function with two identical pointers (e.g., WATCOM C++ (Power++)).
	 */
	if (bhp1->pgno < bhp2->pgno)
		return (-1);
	if (bhp1->pgno > bhp2->pgno)
		return (1);
	return (0);
}
