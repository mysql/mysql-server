/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_alloc.c,v 11.7 2000/04/20 21:14:18 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

/*
 * __memp_alloc --
 *	Allocate some space from a cache region.
 *
 * PUBLIC: int __memp_alloc __P((DB_MPOOL *,
 * PUBLIC:     REGINFO *, MPOOLFILE *, size_t, roff_t *, void *));
 */
int
__memp_alloc(dbmp, memreg, mfp, len, offsetp, retp)
	DB_MPOOL *dbmp;
	REGINFO *memreg;
	MPOOLFILE *mfp;
	size_t len;
	roff_t *offsetp;
	void *retp;
{
	BH *bhp, *nbhp;
	MPOOL *c_mp;
	MPOOLFILE *bh_mfp;
	size_t total;
	int nomore, restart, ret, wrote;
	void *p;

	c_mp = memreg->primary;

	/*
	 * If we're allocating a buffer, and the one we're discarding is the
	 * same size, we don't want to waste the time to re-integrate it into
	 * the shared memory free list.  If the DB_MPOOLFILE argument isn't
	 * NULL, we'll compare the underlying page sizes of the two buffers
	 * before free-ing and re-allocating buffers.
	 */
	if (mfp != NULL)
		len = (sizeof(BH) - sizeof(u_int8_t)) + mfp->stat.st_pagesize;

	nomore = 0;
alloc:	if ((ret = __db_shalloc(memreg->addr, len, MUTEX_ALIGN, &p)) == 0) {
		if (offsetp != NULL)
			*offsetp = R_OFFSET(memreg, p);
		*(void **)retp = p;
		return (0);
	}
	if (nomore) {
		__db_err(dbmp->dbenv,
	    "Unable to allocate %lu bytes from mpool shared region: %s\n",
		    (u_long)len, db_strerror(ret));
		return (ret);
	}

retry:	/* Find a buffer we can flush; pure LRU. */
	restart = total = 0;
	for (bhp =
	    SH_TAILQ_FIRST(&c_mp->bhq, __bh); bhp != NULL; bhp = nbhp) {
		nbhp = SH_TAILQ_NEXT(bhp, q, __bh);

		/* Ignore pinned or locked (I/O in progress) buffers. */
		if (bhp->ref != 0 || F_ISSET(bhp, BH_LOCKED))
			continue;

		/* Find the associated MPOOLFILE. */
		bh_mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);

		/* Write the page if it's dirty. */
		if (F_ISSET(bhp, BH_DIRTY)) {
			++bhp->ref;
			if ((ret = __memp_bhwrite(dbmp,
			    bh_mfp, bhp, &restart, &wrote)) != 0)
				return (ret);
			--bhp->ref;

			/*
			 * Another process may have acquired this buffer and
			 * incremented the ref count after we wrote it.
			 */
			if (bhp->ref != 0)
				goto retry;

			/*
			 * If we wrote the page, continue and free the buffer.
			 * We don't have to rewalk the list to acquire the
			 * buffer because it was never available for any other
			 * process to modify it.
			 *
			 * If we didn't write the page, but we discarded and
			 * reacquired the region lock, restart the list walk.
			 *
			 * If we neither wrote the buffer nor discarded the
			 * region lock, continue down the buffer list.
			 */
			if (wrote)
				++c_mp->stat.st_rw_evict;
			else {
				if (restart)
					goto retry;
				continue;
			}
		} else
			++c_mp->stat.st_ro_evict;

		/*
		 * Check to see if the buffer is the size we're looking for.
		 * If it is, simply reuse it.
		 */
		if (mfp != NULL &&
		    mfp->stat.st_pagesize == bh_mfp->stat.st_pagesize) {
			__memp_bhfree(dbmp, bhp, 0);

			if (offsetp != NULL)
				*offsetp = R_OFFSET(memreg, bhp);
			*(void **)retp = bhp;
			return (0);
		}

		/* Note how much space we've freed, and free the buffer. */
		total += __db_shsizeof(bhp);
		__memp_bhfree(dbmp, bhp, 1);

		/*
		 * Retry as soon as we've freed up sufficient space.  If we
		 * have to coalesce of memory to satisfy the request, don't
		 * try until it's likely (possible?) that we'll succeed.
		 */
		if (total >= 3 * len)
			goto alloc;

		/* Restart the walk if we discarded the region lock. */
		if (restart)
			goto retry;
	}
	nomore = 1;
	goto alloc;
}
