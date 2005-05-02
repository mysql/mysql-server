/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_bh.c,v 11.71 2002/09/04 19:06:45 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"
#include "dbinc/log.h"
#include "dbinc/db_page.h"

static int __memp_pgwrite
	   __P((DB_MPOOL *, DB_MPOOLFILE *, DB_MPOOL_HASH *, BH *));
static int __memp_upgrade __P((DB_MPOOL *, DB_MPOOLFILE *, MPOOLFILE *));

/*
 * __memp_bhwrite --
 *	Write the page associated with a given buffer header.
 *
 * PUBLIC: int __memp_bhwrite __P((DB_MPOOL *,
 * PUBLIC:      DB_MPOOL_HASH *, MPOOLFILE *, BH *, int));
 */
int
__memp_bhwrite(dbmp, hp, mfp, bhp, open_extents)
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOLFILE *mfp;
	BH *bhp;
	int open_extents;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	DB_MPREG *mpreg;
	int local_open, incremented, ret;

	dbenv = dbmp->dbenv;
	local_open = incremented = 0;

	/*
	 * If the file has been removed or is a closed temporary file, jump
	 * right ahead and pretend that we've found the file we want -- the
	 * page-write function knows how to handle the fact that we don't have
	 * (or need!) any real file descriptor information.
	 */
	if (F_ISSET(mfp, MP_DEADFILE)) {
		dbmfp = NULL;
		goto found;
	}

	/*
	 * Walk the process' DB_MPOOLFILE list and find a file descriptor for
	 * the file.  We also check that the descriptor is open for writing.
	 * If we find a descriptor on the file that's not open for writing, we
	 * try and upgrade it to make it writeable.  If that fails, we're done.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	for (dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
	    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q))
		if (dbmfp->mfp == mfp) {
			if (F_ISSET(dbmfp, MP_READONLY) &&
			    !F_ISSET(dbmfp, MP_UPGRADE) &&
			    (F_ISSET(dbmfp, MP_UPGRADE_FAIL) ||
			    __memp_upgrade(dbmp, dbmfp, mfp))) {
				MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
				return (EPERM);
			}

			/*
			 * Increment the reference count -- see the comment in
			 * __memp_fclose_int().
			 */
			++dbmfp->ref;
			incremented = 1;
			break;
		}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	if (dbmfp != NULL)
		goto found;

	/*
	 * !!!
	 * It's the caller's choice if we're going to open extent files.
	 */
	if (!open_extents && F_ISSET(mfp, MP_EXTENT))
		return (EPERM);

	/*
	 * !!!
	 * Don't try to attach to temporary files.  There are two problems in
	 * trying to do that.  First, if we have different privileges than the
	 * process that "owns" the temporary file, we might create the backing
	 * disk file such that the owning process couldn't read/write its own
	 * buffers, e.g., memp_trickle running as root creating a file owned
	 * as root, mode 600.  Second, if the temporary file has already been
	 * created, we don't have any way of finding out what its real name is,
	 * and, even if we did, it was already unlinked (so that it won't be
	 * left if the process dies horribly).  This decision causes a problem,
	 * however: if the temporary file consumes the entire buffer cache,
	 * and the owner doesn't flush the buffers to disk, we could end up
	 * with resource starvation, and the memp_trickle thread couldn't do
	 * anything about it.  That's a pretty unlikely scenario, though.
	 *
	 * Note we should never get here when the temporary file in question
	 * has already been closed in another process, in which case it should
	 * be marked MP_DEADFILE.
	 */
	if (F_ISSET(mfp, MP_TEMP))
		return (EPERM);

	/*
	 * It's not a page from a file we've opened.  If the file requires
	 * input/output processing, see if this process has ever registered
	 * information as to how to write this type of file.  If not, there's
	 * nothing we can do.
	 */
	if (mfp->ftype != 0) {
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
		for (mpreg = LIST_FIRST(&dbmp->dbregq);
		    mpreg != NULL; mpreg = LIST_NEXT(mpreg, q))
			if (mpreg->ftype == mfp->ftype)
				break;
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
		if (mpreg == NULL)
			return (EPERM);
	}

	/*
	 * Try and open the file, attaching to the underlying shared area.
	 * Ignore any error, assume it's a permissions problem.
	 *
	 * XXX
	 * There's no negative cache, so we may repeatedly try and open files
	 * that we have previously tried (and failed) to open.
	 */
	if ((ret = dbenv->memp_fcreate(dbenv, &dbmfp, 0)) != 0)
		return (ret);
	if ((ret = __memp_fopen_int(dbmfp, mfp,
	    R_ADDR(dbmp->reginfo, mfp->path_off),
	    0, 0, mfp->stat.st_pagesize)) != 0) {
		(void)dbmfp->close(dbmfp, 0);
		return (ret);
	}
	local_open = 1;

found:	ret = __memp_pgwrite(dbmp, dbmfp, hp, bhp);

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	if (incremented)
		--dbmfp->ref;
	else if (local_open)
		F_SET(dbmfp, MP_FLUSH);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	return (ret);
}

/*
 * __memp_pgread --
 *	Read a page from a file.
 *
 * PUBLIC: int __memp_pgread __P((DB_MPOOLFILE *, DB_MUTEX *, BH *, int));
 */
int
__memp_pgread(dbmfp, mutexp, bhp, can_create)
	DB_MPOOLFILE *dbmfp;
	DB_MUTEX *mutexp;
	BH *bhp;
	int can_create;
{
	DB_IO db_io;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	size_t len, nr, pagesize;
	int ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mfp = dbmfp->mfp;
	pagesize = mfp->stat.st_pagesize;

	/* We should never be called with a dirty or a locked buffer. */
	DB_ASSERT(!F_ISSET(bhp, BH_DIRTY | BH_DIRTY_CREATE | BH_LOCKED));

	/* Lock the buffer and swap the hash bucket lock for the buffer lock. */
	F_SET(bhp, BH_LOCKED | BH_TRASH);
	MUTEX_LOCK(dbenv, &bhp->mutex);
	MUTEX_UNLOCK(dbenv, mutexp);

	/*
	 * Temporary files may not yet have been created.  We don't create
	 * them now, we create them when the pages have to be flushed.
	 */
	nr = 0;
	if (F_ISSET(dbmfp->fhp, DB_FH_VALID)) {
		db_io.fhp = dbmfp->fhp;
		db_io.mutexp = dbmfp->mutexp;
		db_io.pagesize = db_io.bytes = pagesize;
		db_io.pgno = bhp->pgno;
		db_io.buf = bhp->buf;

		/*
		 * The page may not exist; if it doesn't, nr may well be 0,
		 * but we expect the underlying OS calls not to return an
		 * error code in this case.
		 */
		if ((ret = __os_io(dbenv, &db_io, DB_IO_READ, &nr)) != 0)
			goto err;
	}

	if (nr < pagesize) {
		/*
		 * Don't output error messages for short reads.  In particular,
		 * DB recovery processing may request pages never written to
		 * disk or for which only some part have been written to disk,
		 * in which case we won't find the page.  The caller must know
		 * how to handle the error.
		 */
		if (can_create == 0) {
			ret = DB_PAGE_NOTFOUND;
			goto err;
		}

		/* Clear any bytes that need to be cleared. */
		len = mfp->clear_len == 0 ? pagesize : mfp->clear_len;
		memset(bhp->buf, 0, len);

#if defined(DIAGNOSTIC) || defined(UMRW)
		/*
		 * If we're running in diagnostic mode, corrupt any bytes on
		 * the page that are unknown quantities for the caller.
		 */
		if (len < pagesize)
			memset(bhp->buf + len, CLEAR_BYTE, pagesize - len);
#endif
		++mfp->stat.st_page_create;
	} else
		++mfp->stat.st_page_in;

	/* Call any pgin function. */
	ret = mfp->ftype == 0 ? 0 : __memp_pg(dbmfp, bhp, 1);

	/* Unlock the buffer and reacquire the hash bucket lock. */
err:	MUTEX_UNLOCK(dbenv, &bhp->mutex);
	MUTEX_LOCK(dbenv, mutexp);

	/*
	 * If no errors occurred, the data is now valid, clear the BH_TRASH
	 * flag; regardless, clear the lock bit and let other threads proceed.
	 */
	F_CLR(bhp, BH_LOCKED);
	if (ret == 0)
		F_CLR(bhp, BH_TRASH);

	return (ret);
}

/*
 * __memp_pgwrite --
 *	Write a page to a file.
 */
static int
__memp_pgwrite(dbmp, dbmfp, hp, bhp)
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	DB_MPOOL_HASH *hp;
	BH *bhp;
{
	DB_ENV *dbenv;
	DB_IO db_io;
	DB_LSN lsn;
	MPOOLFILE *mfp;
	size_t nw;
	int callpgin, ret;

	dbenv = dbmp->dbenv;
	mfp = dbmfp == NULL ? NULL : dbmfp->mfp;
	callpgin = ret = 0;

	/*
	 * We should never be called with a clean or trash buffer.
	 * The sync code does call us with already locked buffers.
	 */
	DB_ASSERT(F_ISSET(bhp, BH_DIRTY));
	DB_ASSERT(!F_ISSET(bhp, BH_TRASH));

	/*
	 * If we have not already traded the hash bucket lock for the buffer
	 * lock, do so now.
	 */
	if (!F_ISSET(bhp, BH_LOCKED)) {
		F_SET(bhp, BH_LOCKED);
		MUTEX_LOCK(dbenv, &bhp->mutex);
		MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
	}

	/*
	 * It's possible that the underlying file doesn't exist, either
	 * because of an outright removal or because it was a temporary
	 * file that's been closed.
	 *
	 * !!!
	 * Once we pass this point, we know that dbmfp and mfp aren't NULL,
	 * and that we have a valid file reference.
	 */
	if (mfp == NULL || F_ISSET(mfp, MP_DEADFILE))
		goto file_dead;

	/*
	 * If the page is in a file for which we have LSN information, we have
	 * to ensure the appropriate log records are on disk.
	 */
	if (LOGGING_ON(dbenv) && mfp->lsn_off != -1) {
		memcpy(&lsn, bhp->buf + mfp->lsn_off, sizeof(DB_LSN));
		if ((ret = dbenv->log_flush(dbenv, &lsn)) != 0)
			goto err;
	}

#ifdef DIAGNOSTIC
	/*
	 * Verify write-ahead logging semantics.
	 *
	 * !!!
	 * One special case.  There is a single field on the meta-data page,
	 * the last-page-number-in-the-file field, for which we do not log
	 * changes.  If the page was originally created in a database that
	 * didn't have logging turned on, we can see a page marked dirty but
	 * for which no corresponding log record has been written.  However,
	 * the only way that a page can be created for which there isn't a
	 * previous log record and valid LSN is when the page was created
	 * without logging turned on, and so we check for that special-case
	 * LSN value.
	 */
	if (LOGGING_ON(dbenv) && !IS_NOT_LOGGED_LSN(LSN(bhp->buf))) {
		/*
		 * There is a potential race here.  If we are in the midst of
		 * switching log files, it's possible we could test against the
		 * old file and the new offset in the log region's LSN.  If we
		 * fail the first test, acquire the log mutex and check again.
		 */
		DB_LOG *dblp;
		LOG *lp;

		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		if (!IS_NOT_LOGGED_LSN(LSN(bhp->buf)) &&
		    log_compare(&lp->s_lsn, &LSN(bhp->buf)) <= 0) {
			R_LOCK(dbenv, &dblp->reginfo);
			DB_ASSERT(log_compare(&lp->s_lsn, &LSN(bhp->buf)) > 0);
			R_UNLOCK(dbenv, &dblp->reginfo);
		}
	}
#endif

	/*
	 * Call any pgout function.  We set the callpgin flag so that we flag
	 * that the contents of the buffer will need to be passed through pgin
	 * before they are reused.
	 */
	if (mfp->ftype != 0) {
		callpgin = 1;
		if ((ret = __memp_pg(dbmfp, bhp, 0)) != 0)
			goto err;
	}

	/* Temporary files may not yet have been created. */
	if (!F_ISSET(dbmfp->fhp, DB_FH_VALID)) {
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
		ret = F_ISSET(dbmfp->fhp, DB_FH_VALID) ? 0 :
		    __db_appname(dbenv, DB_APP_TMP, NULL,
		    F_ISSET(dbenv, DB_ENV_DIRECT_DB) ? DB_OSO_DIRECT : 0,
		    dbmfp->fhp, NULL);
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
		if (ret != 0) {
			__db_err(dbenv,
			    "unable to create temporary backing file");
			goto err;
		}
	}

	/* Write the page. */
	db_io.fhp = dbmfp->fhp;
	db_io.mutexp = dbmfp->mutexp;
	db_io.pagesize = db_io.bytes = mfp->stat.st_pagesize;
	db_io.pgno = bhp->pgno;
	db_io.buf = bhp->buf;
	if ((ret = __os_io(dbenv, &db_io, DB_IO_WRITE, &nw)) != 0) {
		__db_err(dbenv, "%s: write failed for page %lu",
		    __memp_fn(dbmfp), (u_long)bhp->pgno);
		goto err;
	}
	++mfp->stat.st_page_out;

err:
file_dead:
	/*
	 * !!!
	 * Once we pass this point, dbmfp and mfp may be NULL, we may not have
	 * a valid file reference.
	 *
	 * Unlock the buffer and reacquire the hash lock.
	 */
	MUTEX_UNLOCK(dbenv, &bhp->mutex);
	MUTEX_LOCK(dbenv, &hp->hash_mutex);

	/*
	 * If we rewrote the page, it will need processing by the pgin
	 * routine before reuse.
	 */
	if (callpgin)
		F_SET(bhp, BH_CALLPGIN);

	/*
	 * Update the hash bucket statistics, reset the flags.
	 * If we were successful, the page is no longer dirty.
	 */
	if (ret == 0) {
		DB_ASSERT(hp->hash_page_dirty != 0);
		--hp->hash_page_dirty;

		F_CLR(bhp, BH_DIRTY | BH_DIRTY_CREATE);
	}

	/* Regardless, clear any sync wait-for count and remove our lock. */
	bhp->ref_sync = 0;
	F_CLR(bhp, BH_LOCKED);

	return (ret);
}

/*
 * __memp_pg --
 *	Call the pgin/pgout routine.
 *
 * PUBLIC: int __memp_pg __P((DB_MPOOLFILE *, BH *, int));
 */
int
__memp_pg(dbmfp, bhp, is_pgin)
	DB_MPOOLFILE *dbmfp;
	BH *bhp;
	int is_pgin;
{
	DBT dbt, *dbtp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPREG *mpreg;
	MPOOLFILE *mfp;
	int ftype, ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mfp = dbmfp->mfp;

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);

	ftype = mfp->ftype;
	for (mpreg = LIST_FIRST(&dbmp->dbregq);
	    mpreg != NULL; mpreg = LIST_NEXT(mpreg, q)) {
		if (ftype != mpreg->ftype)
			continue;
		if (mfp->pgcookie_len == 0)
			dbtp = NULL;
		else {
			dbt.size = mfp->pgcookie_len;
			dbt.data = R_ADDR(dbmp->reginfo, mfp->pgcookie_off);
			dbtp = &dbt;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

		if (is_pgin) {
			if (mpreg->pgin != NULL &&
			    (ret = mpreg->pgin(dbenv,
			    bhp->pgno, bhp->buf, dbtp)) != 0)
				goto err;
		} else
			if (mpreg->pgout != NULL &&
			    (ret = mpreg->pgout(dbenv,
			    bhp->pgno, bhp->buf, dbtp)) != 0)
				goto err;
		break;
	}

	if (mpreg == NULL)
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	return (0);

err:	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
	__db_err(dbenv, "%s: %s failed for page %lu",
	    __memp_fn(dbmfp), is_pgin ? "pgin" : "pgout", (u_long)bhp->pgno);
	return (ret);
}

/*
 * __memp_bhfree --
 *	Free a bucket header and its referenced data.
 *
 * PUBLIC: void __memp_bhfree __P((DB_MPOOL *, DB_MPOOL_HASH *, BH *, int));
 */
void
__memp_bhfree(dbmp, hp, bhp, free_mem)
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	BH *bhp;
	int free_mem;
{
	DB_ENV *dbenv;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	u_int32_t n_cache;

	/*
	 * Assumes the hash bucket is locked and the MPOOL is not.
	 */
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	n_cache = NCACHE(mp, bhp->mf_offset, bhp->pgno);

	/*
	 * Delete the buffer header from the hash bucket queue and reset
	 * the hash bucket's priority, if necessary.
	 */
	SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
	if (bhp->priority == hp->hash_priority)
		hp->hash_priority =
		    SH_TAILQ_FIRST(&hp->hash_bucket, __bh) == NULL ?
		    0 : SH_TAILQ_FIRST(&hp->hash_bucket, __bh)->priority;

	/*
	 * Discard the hash bucket's mutex, it's no longer needed, and
	 * we don't want to be holding it when acquiring other locks.
	 */
	MUTEX_UNLOCK(dbenv, &hp->hash_mutex);

	/*
	 * Find the underlying MPOOLFILE and decrement its reference count.
	 * If this is its last reference, remove it.
	 */
	mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
	MUTEX_LOCK(dbenv, &mfp->mutex);
	if (--mfp->block_cnt == 0 && mfp->mpf_cnt == 0)
		__memp_mf_discard(dbmp, mfp);
	else
		MUTEX_UNLOCK(dbenv, &mfp->mutex);

	R_LOCK(dbenv, &dbmp->reginfo[n_cache]);

	/*
	 * Clear the mutex this buffer recorded; requires the region lock
	 * be held.
	 */
	__db_shlocks_clear(&bhp->mutex, &dbmp->reginfo[n_cache],
	    (REGMAINT *)R_ADDR(&dbmp->reginfo[n_cache], mp->maint_off));

	/*
	 * If we're not reusing the buffer immediately, free the buffer header
	 * and data for real.
	 */
	if (free_mem) {
		__db_shalloc_free(dbmp->reginfo[n_cache].addr, bhp);
		c_mp = dbmp->reginfo[n_cache].primary;
		c_mp->stat.st_pages--;
	}
	R_UNLOCK(dbenv, &dbmp->reginfo[n_cache]);
}

/*
 * __memp_upgrade --
 *	Upgrade a file descriptor from read-only to read-write.
 */
static int
__memp_upgrade(dbmp, dbmfp, mfp)
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	MPOOLFILE *mfp;
{
	DB_ENV *dbenv;
	DB_FH *fhp, *tfhp;
	int ret;
	char *rpath;

	dbenv = dbmp->dbenv;
	fhp = NULL;
	rpath = NULL;

	/*
	 * Calculate the real name for this file and try to open it read/write.
	 * We know we have a valid pathname for the file because it's the only
	 * way we could have gotten a file descriptor of any kind.
	 */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &fhp)) != 0)
		goto err;

	if ((ret = __db_appname(dbenv, DB_APP_DATA,
	    R_ADDR(dbmp->reginfo, mfp->path_off), 0, NULL, &rpath)) != 0)
		goto err;

	if (__os_open(dbenv, rpath,
	    F_ISSET(mfp, MP_DIRECT) ? DB_OSO_DIRECT : 0, 0, fhp) != 0) {
		F_SET(dbmfp, MP_UPGRADE_FAIL);
		goto err;
	}

	/*
	 * Swap the descriptors and set the upgrade flag.
	 *
	 * XXX
	 * There is a race here.  If another process schedules a read using the
	 * existing file descriptor and is swapped out before making the system
	 * call, this code could theoretically close the file descriptor out
	 * from under it.  While it's very unlikely, this code should still be
	 * rewritten.
	 */
	tfhp = dbmfp->fhp;
	dbmfp->fhp = fhp;
	fhp = tfhp;

	(void)__os_closehandle(dbenv, fhp);
	F_SET(dbmfp, MP_UPGRADE);

	ret = 0;
	if (0) {
err:		ret = 1;
	}
	if (fhp != NULL)
		__os_free(dbenv, fhp);
	if (rpath != NULL)
		__os_free(dbenv, rpath);

	return (ret);
}
