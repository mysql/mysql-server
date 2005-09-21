/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_alloc.c,v 11.47 2004/10/15 16:59:42 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

static void __memp_bad_buffer __P((DB_MPOOL_HASH *));

/*
 * __memp_alloc --
 *	Allocate some space from a cache region.
 *
 * PUBLIC: int __memp_alloc __P((DB_MPOOL *,
 * PUBLIC:     REGINFO *, MPOOLFILE *, size_t, roff_t *, void *));
 */
int
__memp_alloc(dbmp, infop, mfp, len, offsetp, retp)
	DB_MPOOL *dbmp;
	REGINFO *infop;
	MPOOLFILE *mfp;
	size_t len;
	roff_t *offsetp;
	void *retp;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL_HASH *dbht, *hp, *hp_end, *hp_tmp;
	DB_MUTEX *mutexp;
	MPOOL *c_mp;
	MPOOLFILE *bh_mfp;
	size_t freed_space;
	u_int32_t buckets, buffers, high_priority, priority, put_counter;
	u_int32_t total_buckets;
	int aggressive, giveup, ret;
	void *p;

	dbenv = dbmp->dbenv;
	c_mp = infop->primary;
	dbht = R_ADDR(infop, c_mp->htab);
	hp_end = &dbht[c_mp->htab_buckets];

	buckets = buffers = put_counter = total_buckets = 0;
	aggressive = giveup = 0;
	hp_tmp = NULL;

	c_mp->stat.st_alloc++;

	/*
	 * If we're allocating a buffer, and the one we're discarding is the
	 * same size, we don't want to waste the time to re-integrate it into
	 * the shared memory free list.  If the DB_MPOOLFILE argument isn't
	 * NULL, we'll compare the underlying page sizes of the two buffers
	 * before free-ing and re-allocating buffers.
	 */
	if (mfp != NULL)
		len = (sizeof(BH) - sizeof(u_int8_t)) + mfp->stat.st_pagesize;

	R_LOCK(dbenv, infop);
	/*
	 * Anything newer than 1/10th of the buffer pool is ignored during
	 * allocation (unless allocation starts failing).
	 */
	high_priority = c_mp->lru_count - c_mp->stat.st_pages / 10;

	/*
	 * First we try to allocate from free memory.  If that fails, scan the
	 * buffer pool to find buffers with low priorities.  We consider small
	 * sets of hash buckets each time to limit the amount of work needing
	 * to be done.  This approximates LRU, but not very well.  We either
	 * find a buffer of the same size to use, or we will free 3 times what
	 * we need in the hopes it will coalesce into a contiguous chunk of the
	 * right size.  In the latter case we branch back here and try again.
	 */
alloc:	if ((ret = __db_shalloc(infop, len, MUTEX_ALIGN, &p)) == 0) {
		if (mfp != NULL)
			c_mp->stat.st_pages++;
		R_UNLOCK(dbenv, infop);

found:		if (offsetp != NULL)
			*offsetp = R_OFFSET(infop, p);
		*(void **)retp = p;

		/*
		 * Update the search statistics.
		 *
		 * We're not holding the region locked here, these statistics
		 * can't be trusted.
		 */
		total_buckets += buckets;
		if (total_buckets != 0) {
			if (total_buckets > c_mp->stat.st_alloc_max_buckets)
				c_mp->stat.st_alloc_max_buckets = total_buckets;
			c_mp->stat.st_alloc_buckets += total_buckets;
		}
		if (buffers != 0) {
			if (buffers > c_mp->stat.st_alloc_max_pages)
				c_mp->stat.st_alloc_max_pages = buffers;
			c_mp->stat.st_alloc_pages += buffers;
		}
		return (0);
	} else if (giveup || c_mp->stat.st_pages == 0) {
		R_UNLOCK(dbenv, infop);

		__db_err(dbenv,
		    "unable to allocate space from the buffer cache");
		return (ret);
	}

	/*
	 * We re-attempt the allocation every time we've freed 3 times what
	 * we need.  Reset our free-space counter.
	 */
	freed_space = 0;
	total_buckets += buckets;
	buckets = 0;

	/*
	 * Walk the hash buckets and find the next two with potentially useful
	 * buffers.  Free the buffer with the lowest priority from the buckets'
	 * chains.
	 */
	for (;;) {
		/* All pages have been freed, make one last try */
		if (c_mp->stat.st_pages == 0)
			goto alloc;

		/* Check for wrap around. */
		hp = &dbht[c_mp->last_checked++];
		if (hp >= hp_end) {
			c_mp->last_checked = 0;
			hp = &dbht[c_mp->last_checked++];
		}

		/*
		 * Skip empty buckets.
		 *
		 * We can check for empty buckets before locking as we
		 * only care if the pointer is zero or non-zero.
		 */
		if (SH_TAILQ_FIRST(&hp->hash_bucket, __bh) == NULL)
			continue;

		/*
		 * The failure mode is when there are too many buffers we can't
		 * write or there's not enough memory in the system.  We don't
		 * have a way to know that allocation has no way to succeed.
		 * We fail if there were no pages returned to the cache after
		 * we've been trying for a relatively long time.
		 *
		 * Get aggressive if we've tried to flush the number of hash
		 * buckets as are in the system and have not found any more
		 * space.  Aggressive means:
		 *
		 * a: set a flag to attempt to flush high priority buffers as
		 *    well as other buffers.
		 * b: sync the mpool to force out queue extent pages.  While we
		 *    might not have enough space for what we want and flushing
		 *    is expensive, why not?
		 * c: look at a buffer in every hash bucket rather than choose
		 *    the more preferable of two.
		 * d: start to think about giving up.
		 *
		 * If we get here twice, sleep for a second, hopefully someone
		 * else will run and free up some memory.
		 *
		 * Always try to allocate memory too, in case some other thread
		 * returns its memory to the region.
		 *
		 * !!!
		 * This test ignores pathological cases like no buffers in the
		 * system -- that shouldn't be possible.
		 */
		if ((++buckets % c_mp->htab_buckets) == 0) {
			if (freed_space > 0)
				goto alloc;
			R_UNLOCK(dbenv, infop);

			switch (++aggressive) {
			case 1:
				break;
			case 2:
				put_counter = c_mp->put_counter;
				/* FALLTHROUGH */
			case 3:
			case 4:
			case 5:
			case 6:
				(void)__memp_sync_int(
				    dbenv, NULL, 0, DB_SYNC_ALLOC, NULL);

				__os_sleep(dbenv, 1, 0);
				break;
			default:
				aggressive = 1;
				if (put_counter == c_mp->put_counter)
					giveup = 1;
				break;
			}

			R_LOCK(dbenv, infop);
			goto alloc;
		}

		if (!aggressive) {
			/* Skip high priority buckets. */
			if (hp->hash_priority > high_priority)
				continue;

			/*
			 * Find two buckets and select the one with the lowest
			 * priority.  Performance testing shows that looking
			 * at two improves the LRUness and looking at more only
			 * does a little better.
			 */
			if (hp_tmp == NULL) {
				hp_tmp = hp;
				continue;
			}
			if (hp->hash_priority > hp_tmp->hash_priority)
				hp = hp_tmp;
			hp_tmp = NULL;
		}

		/* Remember the priority of the buffer we're looking for. */
		priority = hp->hash_priority;

		/* Unlock the region and lock the hash bucket. */
		R_UNLOCK(dbenv, infop);
		mutexp = &hp->hash_mutex;
		MUTEX_LOCK(dbenv, mutexp);

#ifdef DIAGNOSTIC
		__memp_check_order(hp);
#endif
		/*
		 * The lowest priority page is first in the bucket, as they are
		 * maintained in sorted order.
		 *
		 * The buffer may have been freed or its priority changed while
		 * we switched from the region lock to the hash lock.  If so,
		 * we have to restart.  We will still take the first buffer on
		 * the bucket's list, though, if it has a low enough priority.
		 */
		if ((bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) == NULL ||
		    bhp->ref != 0 || bhp->priority > priority)
			goto next_hb;

		buffers++;

		/* Find the associated MPOOLFILE. */
		bh_mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);

		/* If the page is dirty, pin it and write it. */
		ret = 0;
		if (F_ISSET(bhp, BH_DIRTY)) {
			++bhp->ref;
			ret = __memp_bhwrite(dbmp, hp, bh_mfp, bhp, 0);
			--bhp->ref;
			if (ret == 0)
				++c_mp->stat.st_rw_evict;
		} else
			++c_mp->stat.st_ro_evict;

		/*
		 * If a write fails for any reason, we can't proceed.
		 *
		 * We released the hash bucket lock while doing I/O, so another
		 * thread may have acquired this buffer and incremented the ref
		 * count after we wrote it, in which case we can't have it.
		 *
		 * If there's a write error and we're having problems finding
		 * something to allocate, avoid selecting this buffer again
		 * by making it the bucket's least-desirable buffer.
		 */
		if (ret != 0 || bhp->ref != 0) {
			if (ret != 0 && aggressive)
				__memp_bad_buffer(hp);
			goto next_hb;
		}

		/*
		 * Check to see if the buffer is the size we're looking for.
		 * If so, we can simply reuse it.  Else, free the buffer and
		 * its space and keep looking.
		 */
		if (mfp != NULL &&
		    mfp->stat.st_pagesize == bh_mfp->stat.st_pagesize) {
			__memp_bhfree(dbmp, hp, bhp, 0);

			p = bhp;
			goto found;
		}

		freed_space += __db_shalloc_sizeof(bhp);
		__memp_bhfree(dbmp, hp, bhp, BH_FREE_FREEMEM);
		if (aggressive > 1)
			aggressive = 1;

		/*
		 * Unlock this hash bucket and re-acquire the region lock. If
		 * we're reaching here as a result of calling memp_bhfree, the
		 * hash bucket lock has already been discarded.
		 */
		if (0) {
next_hb:		MUTEX_UNLOCK(dbenv, mutexp);
		}
		R_LOCK(dbenv, infop);

		/*
		 * Retry the allocation as soon as we've freed up sufficient
		 * space.  We're likely to have to coalesce of memory to
		 * satisfy the request, don't try until it's likely (possible?)
		 * we'll succeed.
		 */
		if (freed_space >= 3 * len)
			goto alloc;
	}
	/* NOTREACHED */
}

/*
 * __memp_bad_buffer --
 *	Make the first buffer in a hash bucket the least desirable buffer.
 */
static void
__memp_bad_buffer(hp)
	DB_MPOOL_HASH *hp;
{
	BH *bhp;
	u_int32_t priority;

	/* Remove the first buffer from the bucket. */
	bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
	SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);

	/*
	 * Find the highest priority buffer in the bucket.  Buffers are
	 * sorted by priority, so it's the last one in the bucket.
	 */
	priority = bhp->priority;
	if (!SH_TAILQ_EMPTY(&hp->hash_bucket))
	  priority = SH_TAILQ_LAST(&hp->hash_bucket, hq, __bh)->priority;

	/*
	 * Set our buffer's priority to be just as bad, and append it to
	 * the bucket.
	 */
	bhp->priority = priority;
	SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);

	/* Reset the hash bucket's priority. */
	hp->hash_priority = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)->priority;
}

#ifdef DIAGNOSTIC
/*
 * __memp_check_order --
 *	Verify the priority ordering of a hash bucket chain.
 *
 * PUBLIC: #ifdef DIAGNOSTIC
 * PUBLIC: void __memp_check_order __P((DB_MPOOL_HASH *));
 * PUBLIC: #endif
 */
void
__memp_check_order(hp)
	DB_MPOOL_HASH *hp;
{
	BH *bhp;
	u_int32_t priority;

	/*
	 * Assumes the hash bucket is locked.
	 */
	if ((bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) == NULL)
		return;

	DB_ASSERT(bhp->priority == hp->hash_priority);

	for (priority = bhp->priority;
	    (bhp = SH_TAILQ_NEXT(bhp, hq, __bh)) != NULL;
	    priority = bhp->priority)
		DB_ASSERT(priority <= bhp->priority);
}
#endif
