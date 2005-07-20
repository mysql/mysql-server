/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_sync.c,v 11.98 2004/10/15 16:59:43 bostic Exp $
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

typedef struct {
	DB_MPOOL_HASH *track_hp;	/* Hash bucket. */

	roff_t	  track_off;		/* Page file offset. */
	db_pgno_t track_pgno;		/* Page number. */
} BH_TRACK;

static int __bhcmp __P((const void *, const void *));
static int __memp_close_flush_files __P((DB_ENV *, DB_MPOOL *, int));
static int __memp_sync_files __P((DB_ENV *, DB_MPOOL *));

/*
 * __memp_sync_pp --
 *	DB_ENV->memp_sync pre/post processing.
 *
 * PUBLIC: int __memp_sync_pp __P((DB_ENV *, DB_LSN *));
 */
int
__memp_sync_pp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_sync", DB_INIT_MPOOL);

	/*
	 * If no LSN is provided, flush the entire cache (reasonable usage
	 * even if there's no log subsystem configured).
	 */
	if (lsnp != NULL)
		ENV_REQUIRES_CONFIG(dbenv,
		    dbenv->lg_handle, "memp_sync", DB_INIT_LOG);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_sync(dbenv, lsnp);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __memp_sync --
 *	DB_ENV->memp_sync.
 *
 * PUBLIC: int __memp_sync __P((DB_ENV *, DB_LSN *));
 */
int
__memp_sync(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	int ret;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/* If we've flushed to the requested LSN, return that information. */
	if (lsnp != NULL) {
		R_LOCK(dbenv, dbmp->reginfo);
		if (log_compare(lsnp, &mp->lsn) <= 0) {
			*lsnp = mp->lsn;

			R_UNLOCK(dbenv, dbmp->reginfo);
			return (0);
		}
		R_UNLOCK(dbenv, dbmp->reginfo);
	}

	if ((ret = __memp_sync_int(dbenv, NULL, 0, DB_SYNC_CACHE, NULL)) != 0)
		return (ret);

	if (lsnp != NULL) {
		R_LOCK(dbenv, dbmp->reginfo);
		if (log_compare(lsnp, &mp->lsn) > 0)
			mp->lsn = *lsnp;
		R_UNLOCK(dbenv, dbmp->reginfo);
	}

	return (0);
}

/*
 * __memp_fsync_pp --
 *	DB_MPOOLFILE->sync pre/post processing.
 *
 * PUBLIC: int __memp_fsync_pp __P((DB_MPOOLFILE *));
 */
int
__memp_fsync_pp(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	DB_ENV *dbenv;
	int rep_check, ret;

	dbenv = dbmfp->dbenv;

	PANIC_CHECK(dbenv);
	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, "DB_MPOOLFILE->sync");

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_fsync(dbmfp);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __memp_fsync --
 *	DB_MPOOLFILE->sync.
 *
 * PUBLIC: int __memp_fsync __P((DB_MPOOLFILE *));
 */
int
__memp_fsync(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	MPOOLFILE *mfp;

	mfp = dbmfp->mfp;

	/*
	 * If this handle doesn't have a file descriptor that's open for
	 * writing, or if the file is a temporary, or if the file hasn't
	 * been written since it was flushed, there's no reason to proceed
	 * further.
	 */
	if (F_ISSET(dbmfp, MP_READONLY))
		return (0);

	if (F_ISSET(mfp, MP_TEMP))
		return (0);

	if (mfp->file_written == 0)
		return (0);

	return (__memp_sync_int(dbmfp->dbenv, dbmfp, 0, DB_SYNC_FILE, NULL));
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
	if ((*fhp = dbmfp->fhp) != NULL)
		return (0);

	return (__memp_sync_int(dbmfp->dbenv, dbmfp, 0, DB_SYNC_FILE, NULL));
}

/*
 * __memp_sync_int --
 *	Mpool sync internal function.
 *
 * PUBLIC: int __memp_sync_int __P((DB_ENV *,
 * PUBLIC:     DB_MPOOLFILE *, u_int32_t, db_sync_op, u_int32_t *));
 */
int
__memp_sync_int(dbenv, dbmfp, trickle_max, op, wrotep)
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	u_int32_t trickle_max, *wrotep;
	db_sync_op op;
{
	BH *bhp;
	BH_TRACK *bharray;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	DB_MUTEX *mutexp;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	roff_t last_mf_offset;
	u_int32_t ar_cnt, ar_max, i, n_cache, remaining, wrote;
	int filecnt, hb_lock, maxopenfd, maxwrite, maxwrite_sleep;
	int pass, ret, t_ret, wait_cnt, write_cnt;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;
	last_mf_offset = INVALID_ROFF;
	filecnt = pass = wrote = 0;

	/* Get shared configuration information. */
	R_LOCK(dbenv, dbmp->reginfo);
	maxopenfd = mp->mp_maxopenfd;
	maxwrite = mp->mp_maxwrite;
	maxwrite_sleep = mp->mp_maxwrite_sleep;
	R_UNLOCK(dbenv, dbmp->reginfo);

	/* Assume one dirty page per bucket. */
	ar_max = mp->nreg * mp->htab_buckets;
	if ((ret =
	    __os_malloc(dbenv, ar_max * sizeof(BH_TRACK), &bharray)) != 0)
		return (ret);

	/*
	 * Walk each cache's list of buffers and mark all dirty buffers to be
	 * written and all pinned buffers to be potentially written, depending
	 * on our flags.
	 */
	for (ar_cnt = 0, n_cache = 0; n_cache < mp->nreg; ++n_cache) {
		c_mp = dbmp->reginfo[n_cache].primary;

		hp = R_ADDR(&dbmp->reginfo[n_cache], c_mp->htab);
		for (i = 0; i < c_mp->htab_buckets; i++, hp++) {
			/*
			 * We can check for empty buckets before locking as we
			 * only care if the pointer is zero or non-zero.  We
			 * can ignore empty buckets because we only need write
			 * buffers that were dirty before we started.
			 */
			if (SH_TAILQ_FIRST(&hp->hash_bucket, __bh) == NULL)
				continue;

			MUTEX_LOCK(dbenv, &hp->hash_mutex);
			for (bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
			    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh)) {
				/* Always ignore unreferenced, clean pages. */
				if (bhp->ref == 0 && !F_ISSET(bhp, BH_DIRTY))
					continue;

				/*
				 * Checkpoints have to wait on all pinned pages,
				 * as pages may be marked dirty when returned to
				 * the cache.
				 *
				 * File syncs only wait on pages both pinned and
				 * dirty.  (We don't care if pages are marked
				 * dirty when returned to the cache, that means
				 * there's another writing thread and flushing
				 * the cache for this handle is meaningless.)
				 */
				if (op == DB_SYNC_FILE &&
				    !F_ISSET(bhp, BH_DIRTY))
					continue;

				mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);

				/*
				 * Ignore temporary files -- this means you
				 * can't even flush temporary files by handle.
				 * (Checkpoint doesn't require temporary files
				 * be flushed and the underlying buffer write
				 * write routine may not be able to write it
				 * anyway.)
				 */
				if (F_ISSET(mfp, MP_TEMP))
					continue;

				/*
				 * If we're flushing a specific file, see if
				 * this page is from that file.
				 */
				if (dbmfp != NULL && mfp != dbmfp->mfp)
					continue;

				/*
				 * Ignore files that aren't involved in DB's
				 * transactional operations during checkpoints.
				 */
				if (dbmfp == NULL && mfp->lsn_off == -1)
					continue;

				/* Track the buffer, we want it. */
				bharray[ar_cnt].track_hp = hp;
				bharray[ar_cnt].track_pgno = bhp->pgno;
				bharray[ar_cnt].track_off = bhp->mf_offset;
				ar_cnt++;

				/*
				 * If we run out of space, double and continue.
				 * Don't stop at trickle_max, we want to sort
				 * as large a sample set as possible in order
				 * to minimize disk seeks.
				 */
				if (ar_cnt >= ar_max) {
					if ((ret = __os_realloc(dbenv,
					    (ar_max * 2) * sizeof(BH_TRACK),
					    &bharray)) != 0)
						break;
					ar_max *= 2;
				}
			}
			MUTEX_UNLOCK(dbenv, &hp->hash_mutex);

			if (ret != 0)
				goto err;
		}
	}

	/* If there no buffers to write, we're done. */
	if (ar_cnt == 0)
		goto done;

	/*
	 * Write the buffers in file/page order, trying to reduce seeks by the
	 * filesystem and, when pages are smaller than filesystem block sizes,
	 * reduce the actual number of writes.
	 */
	if (ar_cnt > 1)
		qsort(bharray, ar_cnt, sizeof(BH_TRACK), __bhcmp);

	/*
	 * If we're trickling buffers, only write enough to reach the correct
	 * percentage.
	 */
	if (op == DB_SYNC_TRICKLE && ar_cnt > trickle_max)
		ar_cnt = trickle_max;

	/*
	 * Flush the log.  We have to ensure the log records reflecting the
	 * changes on the database pages we're writing have already made it
	 * to disk.  We still have to check the log each time we write a page
	 * (because pages we are about to write may be modified after we have
	 * flushed the log), but in general this will at least avoid any I/O
	 * on the log's part.
	 */
	if (LOGGING_ON(dbenv) && (ret = __log_flush(dbenv, NULL)) != 0)
		goto err;

	/*
	 * Walk the array, writing buffers.  When we write a buffer, we NULL
	 * out its hash bucket pointer so we don't process a slot more than
	 * once.
	 */
	for (i = pass = write_cnt = 0, remaining = ar_cnt; remaining > 0; ++i) {
		if (i >= ar_cnt) {
			i = 0;
			++pass;
			__os_sleep(dbenv, 1, 0);
		}
		if ((hp = bharray[i].track_hp) == NULL)
			continue;

		/* Lock the hash bucket and find the buffer. */
		mutexp = &hp->hash_mutex;
		MUTEX_LOCK(dbenv, mutexp);
		for (bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
		    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh))
			if (bhp->pgno == bharray[i].track_pgno &&
			    bhp->mf_offset == bharray[i].track_off)
				break;

		/*
		 * If we can't find the buffer we're done, somebody else had
		 * to have written it.
		 *
		 * If the buffer isn't pinned or dirty, we're done, there's
		 * no work needed.
		 */
		if (bhp == NULL || (bhp->ref == 0 && !F_ISSET(bhp, BH_DIRTY))) {
			MUTEX_UNLOCK(dbenv, mutexp);
			--remaining;
			bharray[i].track_hp = NULL;
			continue;
		}

		/*
		 * If the buffer is locked by another thread, ignore it, we'll
		 * come back to it.
		 *
		 * If the buffer is pinned and it's only the first or second
		 * time we have looked at it, ignore it, we'll come back to
		 * it.
		 *
		 * In either case, skip the buffer if we're not required to
		 * write it.
		 */
		if (F_ISSET(bhp, BH_LOCKED) || (bhp->ref != 0 && pass < 2)) {
			MUTEX_UNLOCK(dbenv, mutexp);
			if (op != DB_SYNC_CACHE && op != DB_SYNC_FILE) {
				--remaining;
				bharray[i].track_hp = NULL;
			}
			continue;
		}

		/*
		 * The buffer is either pinned or dirty.
		 *
		 * Set the sync wait-for count, used to count down outstanding
		 * references to this buffer as they are returned to the cache.
		 */
		bhp->ref_sync = bhp->ref;

		/* Pin the buffer into memory and lock it. */
		++bhp->ref;
		F_SET(bhp, BH_LOCKED);
		MUTEX_LOCK(dbenv, &bhp->mutex);

		/*
		 * Unlock the hash bucket and wait for the wait-for count to
		 * go to 0.   No new thread can acquire the buffer because we
		 * have it locked.
		 *
		 * If a thread attempts to re-pin a page, the wait-for count
		 * will never go to 0 (the thread spins on our buffer lock,
		 * while we spin on the thread's ref count).  Give up if we
		 * don't get the buffer in 3 seconds, we can try again later.
		 *
		 * If, when the wait-for count goes to 0, the buffer is found
		 * to be dirty, write it.
		 */
		MUTEX_UNLOCK(dbenv, mutexp);
		for (wait_cnt = 1;
		    bhp->ref_sync != 0 && wait_cnt < 4; ++wait_cnt)
			__os_sleep(dbenv, 1, 0);
		MUTEX_LOCK(dbenv, mutexp);
		hb_lock = 1;

		/*
		 * If we've switched files, check to see if we're configured
		 * to close file descriptors.
		 */
		if (maxopenfd != 0 && bhp->mf_offset != last_mf_offset) {
			if (++filecnt >= maxopenfd) {
				filecnt = 0;
				if ((ret = __memp_close_flush_files(
				    dbenv, dbmp, 1)) != 0)
					break;
			}
			last_mf_offset = bhp->mf_offset;
		}

		/*
		 * If the ref_sync count has gone to 0, we're going to be done
		 * with this buffer no matter what happens.
		 */
		if (bhp->ref_sync == 0) {
			--remaining;
			bharray[i].track_hp = NULL;
		}

		/*
		 * If the ref_sync count has gone to 0 and the buffer is still
		 * dirty, we write it.  We only try to write the buffer once.
		 */
		if (bhp->ref_sync == 0 && F_ISSET(bhp, BH_DIRTY)) {
			hb_lock = 0;
			MUTEX_UNLOCK(dbenv, mutexp);

			mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
			if ((ret = __memp_bhwrite(dbmp, hp, mfp, bhp, 1)) == 0)
				++wrote;
			else
				__db_err(dbenv, "%s: unable to flush page: %lu",
				    __memp_fns(dbmp, mfp), (u_long)bhp->pgno);

			/*
			 * Avoid saturating the disk, sleep once we've done
			 * some number of writes.
			 */
			if (maxwrite != 0 && ++write_cnt >= maxwrite) {
				write_cnt = 0;
				__os_sleep(dbenv, 0, (u_long)maxwrite_sleep);
			}
		}

		/*
		 * If ref_sync count never went to 0, the buffer was written
		 * by another thread, or the write failed, we still have the
		 * buffer locked.
		 *
		 * We may or may not currently hold the hash bucket mutex.  If
		 * the __memp_bhwrite -> __memp_pgwrite call was successful,
		 * then __memp_pgwrite will have swapped the buffer lock for
		 * the hash lock.  All other call paths will leave us without
		 * the hash bucket lock.
		 *
		 * The order of mutexes above was to acquire the buffer lock
		 * while holding the hash bucket lock.  Don't deadlock here,
		 * release the buffer lock and then acquire the hash bucket
		 * lock.
		 */
		if (F_ISSET(bhp, BH_LOCKED)) {
			F_CLR(bhp, BH_LOCKED);
			MUTEX_UNLOCK(dbenv, &bhp->mutex);

			if (!hb_lock)
				MUTEX_LOCK(dbenv, mutexp);
		}

		/*
		 * Reset the ref_sync count regardless of our success, we're
		 * done with this buffer for now.
		 */
		bhp->ref_sync = 0;

		/* Discard our reference and unlock the bucket. */
		--bhp->ref;
		MUTEX_UNLOCK(dbenv, mutexp);

		if (ret != 0)
			break;
	}

done:	/*
	 * If doing a checkpoint or flushing a file for the application, we
	 * have to force the pages to disk.  We don't do this as we go along
	 * because we want to give the OS as much time as possible to lazily
	 * flush, and because we have to flush files that might not even have
	 * had dirty buffers in the cache, so we have to walk the files list.
	 */
	if (ret == 0 && (op == DB_SYNC_CACHE || op == DB_SYNC_FILE)) {
		if (dbmfp == NULL)
			ret = __memp_sync_files(dbenv, dbmp);
		else
			ret = __os_fsync(dbenv, dbmfp->fhp);
	}

	/* If we've opened files to flush pages, close them. */
	if ((t_ret = __memp_close_flush_files(dbenv, dbmp, 0)) != 0 && ret == 0)
		ret = t_ret;

err:	__os_free(dbenv, bharray);
	if (wrotep != NULL)
		*wrotep = wrote;

	return (ret);
}

/*
 * __memp_sync_files --
 *	Sync all the files in the environment, open or not.
 */
static
int __memp_sync_files(dbenv, dbmp)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
{
	DB_MPOOLFILE *dbmfp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	int final_ret, ret;

	final_ret = 0;
	mp = dbmp->reginfo[0].primary;

	R_LOCK(dbenv, dbmp->reginfo);
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
		if (!mfp->file_written ||
		    mfp->deadfile || F_ISSET(mfp, MP_TEMP))
			continue;

		/*
		 * Look for an already open, writeable handle (fsync doesn't
		 * work on read-only Windows handles).
		 */
		ret = 0;
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
		for (dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
		    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q)) {
			if (dbmfp->mfp != mfp || F_ISSET(dbmfp, MP_READONLY))
				continue;
			ret = __os_fsync(dbenv, dbmfp->fhp);
			break;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

		/* If we don't find one, open one. */
		if (dbmfp == NULL)
			ret = __memp_mf_sync(dbmp, mfp);
		if (ret != 0) {
			__db_err(dbenv, "%s: unable to flush: %s",
			    (char *)R_ADDR(dbmp->reginfo, mfp->path_off),
			    db_strerror(ret));
			if (final_ret == 0)
				final_ret = ret;
			continue;
		}

		/*
		 * If we wrote the file and there are no open handles (or there
		 * is a single open handle, and it's the one we opened to write
		 * buffers during checkpoint), clear the file_written flag.  We
		 * do this so that applications opening thousands of files don't
		 * loop here opening and flushing those files during checkpoint.
		 *
		 * The danger here is if a buffer were to be written as part of
		 * a checkpoint, and then not be flushed to disk.  This cannot
		 * happen because we only clear file_written when there are no
		 * other users of the MPOOLFILE in the system, and, as we hold
		 * the region lock, no possibility of another thread of control
		 * racing with us to open a MPOOLFILE.
		 */
		if (mfp->mpf_cnt == 0 || (mfp->mpf_cnt == 1 &&
		    dbmfp != NULL && F_ISSET(dbmfp, MP_FLUSH)))
			mfp->file_written = 0;
	}
	R_UNLOCK(dbenv, dbmp->reginfo);

	return (final_ret);
}

/*
 * __memp_mf_sync --
 *	 Flush an MPOOLFILE.
 *
 *	Should only be used when the file is not already open in this process.
 *
 * PUBLIC: int __memp_mf_sync __P((DB_MPOOL *, MPOOLFILE *));
 */
int
__memp_mf_sync(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	DB_ENV *dbenv;
	DB_FH *fhp;
	int ret, t_ret;
	char *rpath;

	dbenv = dbmp->dbenv;

	/*
	 * Expects caller to be holding the region lock: we're using the path
	 * name and __memp_nameop might try and rename the file.
	 */
	if ((ret = __db_appname(dbenv, DB_APP_DATA,
	    R_ADDR(dbmp->reginfo, mfp->path_off), 0, NULL,
	    &rpath)) == 0) {
		if ((ret = __os_open(dbenv, rpath, 0, 0, &fhp)) == 0) {
			ret = __os_fsync(dbenv, fhp);
			if ((t_ret =
			    __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
				ret = t_ret;
		}
		__os_free(dbenv, rpath);
	}

	return (ret);
}

/*
 * __memp_close_flush_files --
 *	Close files opened only to flush buffers.
 */
static int
__memp_close_flush_files(dbenv, dbmp, dosync)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int dosync;
{
	DB_MPOOLFILE *dbmfp;
	MPOOLFILE *mfp;
	int ret;

	/*
	 * The routine exists because we must close files opened by sync to
	 * flush buffers.  There are two cases: first, extent files have to
	 * be closed so they may be removed when empty.  Second, regular
	 * files have to be closed so we don't run out of descriptors (for
	 * example, an application partitioning its data into databases
	 * based on timestamps, so there's a continually increasing set of
	 * files).
	 *
	 * We mark files opened in the __memp_bhwrite() function with the
	 * MP_FLUSH flag.  Here we walk through our file descriptor list,
	 * and, if a file was opened by __memp_bhwrite(), we close it.
	 */
retry:	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	for (dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
	    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q))
		if (F_ISSET(dbmfp, MP_FLUSH)) {
			F_CLR(dbmfp, MP_FLUSH);
			MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
			if (dosync) {
				if ((ret = __os_fsync(dbenv, dbmfp->fhp)) != 0)
					return (ret);
				/*
				 * If the file is clean and we have the only
				 * open handle on the file, clear the dirty
				 * flag so we don't re-open and sync it again.
				 */
				mfp = dbmfp->mfp;
				if (mfp->mpf_cnt == 1) {
					R_LOCK(dbenv, dbmp->reginfo);
					if (mfp->mpf_cnt == 1)
						mfp->file_written = 0;
					R_UNLOCK(dbenv, dbmp->reginfo);
				}
			}
			if ((ret = __memp_fclose(dbmfp, 0)) != 0)
				return (ret);
			goto retry;
		}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	return (0);
}

static int
__bhcmp(p1, p2)
	const void *p1, *p2;
{
	BH_TRACK *bhp1, *bhp2;

	bhp1 = (BH_TRACK *)p1;
	bhp2 = (BH_TRACK *)p2;

	/* Sort by file (shared memory pool offset). */
	if (bhp1->track_off < bhp2->track_off)
		return (-1);
	if (bhp1->track_off > bhp2->track_off)
		return (1);

	/*
	 * !!!
	 * Defend against badly written quicksort code calling the comparison
	 * function with two identical pointers (e.g., WATCOM C++ (Power++)).
	 */
	if (bhp1->track_pgno < bhp2->track_pgno)
		return (-1);
	if (bhp1->track_pgno > bhp2->track_pgno)
		return (1);
	return (0);
}
