/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fget.c,v 11.28 2001/01/10 04:50:53 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
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
 * memp_fget --
 *	Get a page from the file.
 */
int
memp_fget(dbmfp, pgnoaddr, flags, addrp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
	u_int32_t flags;
	void *addrp;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_HASHTAB *dbht;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	size_t n_bucket, n_cache, mf_offset;
	u_int32_t st_hsearch;
	int b_incr, first, ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	mfp = dbmfp->mfp;
#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fget(dbmfp, pgnoaddr, flags, addrp));
#endif

	PANIC_CHECK(dbenv);

	/*
	 * Validate arguments.
	 *
	 * !!!
	 * Don't test for DB_MPOOL_CREATE and DB_MPOOL_NEW flags for readonly
	 * files here, and create non-existent pages in readonly files if the
	 * flags are set, later.  The reason is that the hash access method
	 * wants to get empty pages that don't really exist in readonly files.
	 * The only alternative is for hash to write the last "bucket" all the
	 * time, which we don't want to do because one of our big goals in life
	 * is to keep database files small.  It's sleazy as hell, but we catch
	 * any attempt to actually write the file in memp_fput().
	 */
#define	OKFLAGS	\
    (DB_MPOOL_CREATE | DB_MPOOL_LAST | \
    DB_MPOOL_NEW | DB_MPOOL_NEW_GROUP | DB_MPOOL_EXTENT)
	if (flags != 0) {
		if ((ret = __db_fchk(dbenv, "memp_fget", flags, OKFLAGS)) != 0)
			return (ret);

		switch (flags & ~DB_MPOOL_EXTENT) {
		case DB_MPOOL_CREATE:
		case DB_MPOOL_LAST:
		case DB_MPOOL_NEW:
		case DB_MPOOL_NEW_GROUP:
		case 0:
			break;
		default:
			return (__db_ferr(dbenv, "memp_fget", 1));
		}
	}

#ifdef DIAGNOSTIC
	/*
	 * XXX
	 * We want to switch threads as often as possible.  Yield every time
	 * we get a new page to ensure contention.
	 */
	if (DB_GLOBAL(db_pageyield))
		__os_yield(dbenv, 1);
#endif

	/* Initialize remaining local variables. */
	mf_offset = R_OFFSET(dbmp->reginfo, mfp);
	bhp = NULL;
	st_hsearch = 0;
	b_incr = ret = 0;

	R_LOCK(dbenv, dbmp->reginfo);

	/*
	 * Check for the new, last or last + 1 page requests.
	 *
	 * Examine and update the file's last_pgno value.  We don't care if
	 * the last_pgno value immediately changes due to another thread --
	 * at this instant in time, the value is correct.  We do increment the
	 * current last_pgno value if the thread is asking for a new page,
	 * however, to ensure that two threads creating pages don't get the
	 * same one.
	 *
	 * If we create a page, there is the potential that a page after it
	 * in the file will be written before it will be written.  Recovery
	 * depends on pages that are "created" in the file by subsequent pages
	 * being written be zeroed out, not have random garbage.  Ensure that
	 * the OS agrees.
	 *
	 * !!!
	 * DB_MPOOL_NEW_GROUP is undocumented -- the hash access method needs
	 * to allocate contiguous groups of pages in order to do subdatabases.
	 * We return the first page in the group, but the caller must put an
	 * LSN on the *last* page and write it, otherwise after a crash we may
	 * not create all of the pages we need to create.
	 */
	if (LF_ISSET(DB_MPOOL_LAST | DB_MPOOL_NEW | DB_MPOOL_NEW_GROUP)) {
		if (LF_ISSET(DB_MPOOL_NEW)) {
			if (F_ISSET(&dbmfp->fh, DB_FH_VALID) && (ret =
			    __os_fpinit(dbenv, &dbmfp->fh, mfp->last_pgno + 1,
			    1, mfp->stat.st_pagesize)) != 0) {
				R_UNLOCK(dbenv, dbmp->reginfo);
				return (ret);
			}
			++mfp->last_pgno;
		}
		if (LF_ISSET(DB_MPOOL_NEW_GROUP)) {
			if (F_ISSET(&dbmfp->fh, DB_FH_VALID) && (ret =
			    __os_fpinit(dbenv, &dbmfp->fh, mfp->last_pgno + 1,
			    (int)*pgnoaddr, mfp->stat.st_pagesize)) != 0) {
				R_UNLOCK(dbenv, dbmp->reginfo);
				return (ret);
			}
			mfp->last_pgno += *pgnoaddr;
		}
		*pgnoaddr = mfp->last_pgno;
	}

	/*
	 * Determine the hash bucket where this page will live, and get local
	 * pointers to the cache and its hash table.
	 */
	n_cache = NCACHE(mp, *pgnoaddr);
	c_mp = dbmp->reginfo[n_cache].primary;
	n_bucket = NBUCKET(c_mp, mf_offset, *pgnoaddr);
	dbht = R_ADDR(&dbmp->reginfo[n_cache], c_mp->htab);

	if (LF_ISSET(DB_MPOOL_NEW | DB_MPOOL_NEW_GROUP))
		goto alloc;

	/*
	 * If mmap'ing the file and the page is not past the end of the file,
	 * just return a pointer.
	 *
	 * The page may be past the end of the file, so check the page number
	 * argument against the original length of the file.  If we previously
	 * returned pages past the original end of the file, last_pgno will
	 * have been updated to match the "new" end of the file, and checking
	 * against it would return pointers past the end of the mmap'd region.
	 *
	 * If another process has opened the file for writing since we mmap'd
	 * it, we will start playing the game by their rules, i.e. everything
	 * goes through the cache.  All pages previously returned will be safe,
	 * as long as the correct locking protocol was observed.
	 *
	 * XXX
	 * We don't discard the map because we don't know when all of the
	 * pages will have been discarded from the process' address space.
	 * It would be possible to do so by reference counting the open
	 * pages from the mmap, but it's unclear to me that it's worth it.
	 */
	if (dbmfp->addr != NULL && F_ISSET(mfp, MP_CAN_MMAP)) {
		if (*pgnoaddr > mfp->orig_last_pgno) {
			/*
			 * !!!
			 * See the comment above about non-existent pages and
			 * the hash access method.
			 */
			if (!LF_ISSET(DB_MPOOL_CREATE)) {
				if (!LF_ISSET(DB_MPOOL_EXTENT))
					__db_err(dbenv,
					    "%s: page %lu doesn't exist",
					    __memp_fn(dbmfp), (u_long)*pgnoaddr);
				ret = EINVAL;
				goto err;
			}
		} else {
			*(void **)addrp =
			    R_ADDR(dbmfp, *pgnoaddr * mfp->stat.st_pagesize);
			++mfp->stat.st_map;
			goto done;
		}
	}

	/* Search the hash chain for the page. */
	for (bhp = SH_TAILQ_FIRST(&dbht[n_bucket], __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh)) {
		++st_hsearch;
		if (bhp->pgno != *pgnoaddr || bhp->mf_offset != mf_offset)
			continue;

		/* Increment the reference count. */
		if (bhp->ref == UINT16_T_MAX) {
			__db_err(dbenv,
			    "%s: page %lu: reference count overflow",
			    __memp_fn(dbmfp), (u_long)bhp->pgno);
			ret = EINVAL;
			goto err;
		}

		/*
		 * Increment the reference count.  We may discard the region
		 * lock as we evaluate and/or read the buffer, so we need to
		 * ensure that it doesn't move and that its contents remain
		 * unchanged.
		 */
		++bhp->ref;
		b_incr = 1;

		/*
		 * Any buffer we find might be trouble.
		 *
		 * BH_LOCKED --
		 * I/O is in progress.  Because we've incremented the buffer
		 * reference count, we know the buffer can't move.  Unlock
		 * the region lock, wait for the I/O to complete, and reacquire
		 * the region.
		 */
		for (first = 1; F_ISSET(bhp, BH_LOCKED); first = 0) {
			R_UNLOCK(dbenv, dbmp->reginfo);

			/*
			 * Explicitly yield the processor if it's not the first
			 * pass through this loop -- if we don't, we might end
			 * up running to the end of our CPU quantum as we will
			 * simply be swapping between the two locks.
			 */
			if (!first)
				__os_yield(dbenv, 1);

			MUTEX_LOCK(dbenv, &bhp->mutex, dbenv->lockfhp);
			/* Wait for I/O to finish... */
			MUTEX_UNLOCK(dbenv, &bhp->mutex);
			R_LOCK(dbenv, dbmp->reginfo);
		}

		/*
		 * BH_TRASH --
		 * The contents of the buffer are garbage.  Shouldn't happen,
		 * and this read is likely to fail, but might as well try.
		 */
		if (F_ISSET(bhp, BH_TRASH))
			goto reread;

		/*
		 * BH_CALLPGIN --
		 * The buffer was converted so it could be written, and the
		 * contents need to be converted again.
		 */
		if (F_ISSET(bhp, BH_CALLPGIN)) {
			if ((ret = __memp_pg(dbmfp, bhp, 1)) != 0)
				goto err;
			F_CLR(bhp, BH_CALLPGIN);
		}

		++mfp->stat.st_cache_hit;
		*(void **)addrp = bhp->buf;
		goto done;
	}

alloc:	/* Allocate new buffer header and data space. */
	if ((ret = __memp_alloc(dbmp,
	    &dbmp->reginfo[n_cache], mfp, 0, NULL, &bhp)) != 0)
		goto err;

	++c_mp->stat.st_page_clean;

	/*
	 * Initialize the BH fields so that we can call the __memp_bhfree
	 * routine if an error occurs.
	 */
	memset(bhp, 0, sizeof(BH));
	bhp->ref = 1;
	bhp->pgno = *pgnoaddr;
	bhp->mf_offset = mf_offset;

	/* Increment the count of buffers referenced by this MPOOLFILE. */
	++mfp->block_cnt;

	/*
	 * Prepend the bucket header to the head of the appropriate MPOOL
	 * bucket hash list.  Append the bucket header to the tail of the
	 * MPOOL LRU chain.
	 */
	SH_TAILQ_INSERT_HEAD(&dbht[n_bucket], bhp, hq, __bh);
	SH_TAILQ_INSERT_TAIL(&c_mp->bhq, bhp, q);

#ifdef DIAGNOSTIC
	if ((db_alignp_t)bhp->buf & (sizeof(size_t) - 1)) {
		__db_err(dbenv, "Internal error: BH data NOT size_t aligned.");
		ret = EINVAL;
		__memp_bhfree(dbmp, bhp, 1);
		goto err;
	}
#endif

	if ((ret = __db_shmutex_init(dbenv, &bhp->mutex,
	    R_OFFSET(dbmp->reginfo, &bhp->mutex) + DB_FCNTL_OFF_MPOOL,
	    0, &dbmp->reginfo[n_cache],
	    (REGMAINT *)R_ADDR(&dbmp->reginfo[n_cache], c_mp->maint_off)))
	    != 0) {
		__memp_bhfree(dbmp, bhp, 1);
		goto err;
	}

	/*
	 * If we created the page, zero it out and continue.
	 *
	 * !!!
	 * Note: DB_MPOOL_NEW specifically doesn't call the pgin function.
	 * If DB_MPOOL_CREATE is used, then the application's pgin function
	 * has to be able to handle pages of 0's -- if it uses DB_MPOOL_NEW,
	 * it can detect all of its page creates, and not bother.
	 *
	 * If we're running in diagnostic mode, smash any bytes on the
	 * page that are unknown quantities for the caller.
	 *
	 * Otherwise, read the page into memory, optionally creating it if
	 * DB_MPOOL_CREATE is set.
	 */
	if (LF_ISSET(DB_MPOOL_NEW | DB_MPOOL_NEW_GROUP)) {
		if (mfp->clear_len == 0)
			memset(bhp->buf, 0, mfp->stat.st_pagesize);
		else {
			memset(bhp->buf, 0, mfp->clear_len);
#ifdef DIAGNOSTIC
			memset(bhp->buf + mfp->clear_len, CLEAR_BYTE,
			    mfp->stat.st_pagesize - mfp->clear_len);
#endif
		}

		++mfp->stat.st_page_create;
	} else {
		/*
		 * It's possible for the read function to fail, which means
		 * that we fail as well.  Note, the __memp_pgread() function
		 * discards the region lock, so the buffer must be pinned
		 * down so that it cannot move and its contents are unchanged.
		 */
reread:		if ((ret = __memp_pgread(dbmfp,
		    bhp, LF_ISSET(DB_MPOOL_CREATE|DB_MPOOL_EXTENT))) != 0) {
			/*
			 * !!!
			 * Discard the buffer unless another thread is waiting
			 * on our I/O to complete.  Regardless, the header has
			 * the BH_TRASH flag set.
			 */
			if (bhp->ref == 1)
				__memp_bhfree(dbmp, bhp, 1);
			goto err;
		}

		++mfp->stat.st_cache_miss;
	}

	/*
	 * If we're returning a page after our current notion of the last-page,
	 * update our information.  Note, there's no way to un-instantiate this
	 * page, it's going to exist whether it's returned to us dirty or not.
	 */
	if (bhp->pgno > mfp->last_pgno)
		mfp->last_pgno = bhp->pgno;

	*(void **)addrp = bhp->buf;

done:	/* Update the chain search statistics. */
	if (st_hsearch) {
		++c_mp->stat.st_hash_searches;
		if (st_hsearch > c_mp->stat.st_hash_longest)
			c_mp->stat.st_hash_longest = st_hsearch;
		c_mp->stat.st_hash_examined += st_hsearch;
	}

	++dbmfp->pinref;

	R_UNLOCK(dbenv, dbmp->reginfo);

	return (0);

err:	/* Discard our reference. */
	if (b_incr)
		--bhp->ref;
	R_UNLOCK(dbenv, dbmp->reginfo);

	*(void **)addrp = NULL;
	return (ret);
}
