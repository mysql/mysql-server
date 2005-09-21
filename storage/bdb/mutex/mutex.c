/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mutex.c,v 11.43 2004/10/15 16:59:44 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

#if	defined(MUTEX_NO_MALLOC_LOCKS) || defined(HAVE_MUTEX_SYSTEM_RESOURCES)
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"
#endif

static int __db_mutex_alloc_int __P((DB_ENV *, REGINFO *, DB_MUTEX **));
#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
static REGMAINT * __db_mutex_maint __P((DB_ENV *, REGINFO *));
#endif

/*
 * __db_mutex_setup --
 *	External interface to allocate, and/or initialize, record
 *	mutexes.
 *
 * PUBLIC: int __db_mutex_setup __P((DB_ENV *, REGINFO *, void *, u_int32_t));
 */
int
__db_mutex_setup(dbenv, infop, ptr, flags)
	DB_ENV *dbenv;
	REGINFO *infop;
	void *ptr;
	u_int32_t flags;
{
	DB_MUTEX *mutex;
	REGMAINT *maint;
	u_int32_t iflags, offset;
	int ret;

	ret = 0;
	/*
	 * If they indicated the region is not locked, then lock it.
	 * This is only needed when we have unusual mutex resources.
	 * (I.e. MUTEX_NO_MALLOC_LOCKS or HAVE_MUTEX_SYSTEM_RESOURCES)
	 */
#if	defined(MUTEX_NO_MALLOC_LOCKS) || defined(HAVE_MUTEX_SYSTEM_RESOURCES)
	if (!LF_ISSET(MUTEX_NO_RLOCK))
		R_LOCK(dbenv, infop);
#endif
	/*
	 * Allocate the mutex if they asked us to.
	 */
	mutex = NULL;
	if (LF_ISSET(MUTEX_ALLOC)) {
		if ((ret = __db_mutex_alloc_int(dbenv, infop, ptr)) != 0)
			goto err;
		mutex = *(DB_MUTEX **)ptr;
	} else
		mutex = (DB_MUTEX *)ptr;

	/*
	 * Set up to initialize the mutex.
	 */
	iflags = LF_ISSET(MUTEX_LOGICAL_LOCK | MUTEX_THREAD | MUTEX_SELF_BLOCK);
	switch (infop->type) {
	case REGION_TYPE_LOCK:
		offset = P_TO_UINT32(mutex) + DB_FCNTL_OFF_LOCK;
		break;
	case REGION_TYPE_MPOOL:
		offset = P_TO_UINT32(mutex) + DB_FCNTL_OFF_MPOOL;
		break;
	default:
		offset = P_TO_UINT32(mutex) + DB_FCNTL_OFF_GEN;
		break;
	}
	maint = NULL;
#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
	if (!LF_ISSET(MUTEX_NO_RECORD))
		maint = (REGMAINT *)__db_mutex_maint(dbenv, infop);
#endif

	ret = __db_mutex_init(dbenv, mutex, offset, iflags, infop, maint);
err:
#if	defined(MUTEX_NO_MALLOC_LOCKS) || defined(HAVE_MUTEX_SYSTEM_RESOURCES)
	if (!LF_ISSET(MUTEX_NO_RLOCK))
		R_UNLOCK(dbenv, infop);
#endif
	/*
	 * If we allocated the mutex but had an error on init'ing,
	 * then we must free it before returning.
	 * !!!
	 * Free must be done after releasing region lock.
	 */
	if (ret != 0 && LF_ISSET(MUTEX_ALLOC) && mutex != NULL) {
		__db_mutex_free(dbenv, infop, mutex);
		*(DB_MUTEX **)ptr = NULL;
	}
	return (ret);
}

/*
 * __db_mutex_alloc_int --
 *	Allocate and initialize a mutex.
 */
static int
__db_mutex_alloc_int(dbenv, infop, storep)
	DB_ENV *dbenv;
	REGINFO *infop;
	DB_MUTEX **storep;
{
	int ret;

	/*
	 * If the architecture supports mutexes in heap memory, use heap memory.
	 * If it doesn't, we have to allocate space in a region.  If allocation
	 * in the region fails, fallback to allocating from the mpool region,
	 * because it's big, it almost always exists and if it's entirely dirty,
	 * we can free buffers until memory is available.
	 */
#if	defined(MUTEX_NO_MALLOC_LOCKS) || defined(HAVE_MUTEX_SYSTEM_RESOURCES)
	ret = __db_shalloc(infop, sizeof(DB_MUTEX), MUTEX_ALIGN, storep);

	if (ret == ENOMEM && MPOOL_ON(dbenv)) {
		DB_MPOOL *dbmp;

		dbmp = dbenv->mp_handle;
		if ((ret = __memp_alloc(dbmp,
		    dbmp->reginfo, NULL, sizeof(DB_MUTEX), NULL, storep)) == 0)
			(*storep)->flags = MUTEX_MPOOL;
	} else
		(*storep)->flags = 0;
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
	ret = __os_calloc(dbenv, 1, sizeof(DB_MUTEX), storep);
#endif
	if (ret != 0)
		__db_err(dbenv, "Unable to allocate memory for mutex");
	return (ret);
}

/*
 * __db_mutex_free --
 *	Free a mutex.
 *
 * PUBLIC: void __db_mutex_free __P((DB_ENV *, REGINFO *, DB_MUTEX *));
 */
void
__db_mutex_free(dbenv, infop, mutexp)
	DB_ENV *dbenv;
	REGINFO *infop;
	DB_MUTEX *mutexp;
{
#if	defined(MUTEX_NO_MALLOC_LOCKS) || defined(HAVE_MUTEX_SYSTEM_RESOURCES)
	R_LOCK(dbenv, infop);
#if	defined(HAVE_MUTEX_SYSTEM_RESOURCES)
	if (F_ISSET(mutexp, MUTEX_INITED))
		__db_shlocks_clear(mutexp, infop, NULL);
#endif
	if (F_ISSET(mutexp, MUTEX_MPOOL)) {
		DB_MPOOL *dbmp;

		dbmp = dbenv->mp_handle;
		R_LOCK(dbenv, dbmp->reginfo);
		__db_shalloc_free(&dbmp->reginfo[0], mutexp);
		R_UNLOCK(dbenv, dbmp->reginfo);
	} else
		__db_shalloc_free(infop, mutexp);
	R_UNLOCK(dbenv, infop);
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
	__os_free(dbenv, mutexp);
#endif
}

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
/*
 * __db_shreg_locks_record --
 *	Record an entry in the shared locks area.
 *	Region lock must be held in caller.
 */
static int
__db_shreg_locks_record(dbenv, mutexp, infop, rp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	REGINFO *infop;
	REGMAINT *rp;
{
	u_int i;

	if (!F_ISSET(mutexp, MUTEX_INITED))
		return (0);
	DB_ASSERT(mutexp->reg_off == INVALID_ROFF);
	rp->stat.st_records++;
	i = (roff_t *)R_ADDR(infop, rp->regmutex_hint) - &rp->regmutexes[0];
	if (rp->regmutexes[i] != INVALID_ROFF) {
		/*
		 * Our hint failed, search for an open slot.
		 */
		rp->stat.st_hint_miss++;
		for (i = 0; i < rp->reglocks; i++)
			if (rp->regmutexes[i] == INVALID_ROFF)
				break;
		if (i == rp->reglocks) {
			rp->stat.st_max_locks++;
			__db_err(dbenv,
			    "Region mutexes: Exceeded maximum lock slots %lu",
			    (u_long)rp->reglocks);
			return (ENOMEM);
		}
	} else
		rp->stat.st_hint_hit++;
	/*
	 * When we get here, i is an empty slot.  Record this
	 * mutex, set hint to point to the next slot and we are done.
	 */
	rp->regmutexes[i] = R_OFFSET(infop, mutexp);
	mutexp->reg_off = R_OFFSET(infop, &rp->regmutexes[i]);
	rp->regmutex_hint = (i < rp->reglocks - 1) ?
	    R_OFFSET(infop, &rp->regmutexes[i+1]) :
	    R_OFFSET(infop, &rp->regmutexes[0]);
	return (0);
}

/*
 * __db_shreg_locks_clear --
 *	Erase an entry in the shared locks area.
 *
 * PUBLIC: void __db_shreg_locks_clear __P((DB_MUTEX *, REGINFO *, REGMAINT *));
 */
void
__db_shreg_locks_clear(mutexp, infop, rp)
	DB_MUTEX *mutexp;
	REGINFO *infop;
	REGMAINT *rp;
{
	/*
	 * !!!
	 * Assumes the caller's region lock is held.
	 */
	if (!F_ISSET(mutexp, MUTEX_INITED))
		return;
	/*
	 * This function is generally only called on a forcible remove of an
	 * environment.  We recorded our index in the mutex, find and clear it.
	 */
	DB_ASSERT(mutexp->reg_off != INVALID_ROFF);
	DB_ASSERT(*(roff_t *)R_ADDR(infop, mutexp->reg_off) == \
	    R_OFFSET(infop, mutexp));
	*(roff_t *)R_ADDR(infop, mutexp->reg_off) = 0;
	if (rp != NULL) {
		rp->regmutex_hint = mutexp->reg_off;
		rp->stat.st_clears++;
	}
	mutexp->reg_off = INVALID_ROFF;
	__db_mutex_destroy(mutexp);
}

/*
 * __db_shreg_locks_destroy --
 *	Destroy all mutexes in a region's range.
 *
 * PUBLIC: void __db_shreg_locks_destroy __P((REGINFO *, REGMAINT *));
 */
void
__db_shreg_locks_destroy(infop, rp)
	REGINFO *infop;
	REGMAINT *rp;
{
	u_int32_t i;

	/*
	 * Go through the list of all mutexes and destroy them.
	 */
	for (i = 0; i < rp->reglocks; i++)
		if (rp->regmutexes[i] != 0) {
			rp->stat.st_destroys++;
			__db_mutex_destroy(R_ADDR(infop, rp->regmutexes[i]));
		}
}

/*
 * __db_shreg_mutex_init --
 *	Initialize a shared memory mutex.
 *
 * PUBLIC: int __db_shreg_mutex_init __P((DB_ENV *, DB_MUTEX *, u_int32_t,
 * PUBLIC:    u_int32_t, REGINFO *, REGMAINT *));
 */
int
__db_shreg_mutex_init(dbenv, mutexp, offset, flags, infop, rp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	u_int32_t offset;
	u_int32_t flags;
	REGINFO *infop;
	REGMAINT *rp;
{
	int ret;

	if ((ret = __db_mutex_init_int(dbenv, mutexp, offset, flags)) != 0)
		return (ret);
	/*
	 * Some mutexes cannot be recorded, but we want one interface.
	 * So, if we have no REGMAINT, then just return.
	 */
	if (rp == NULL)
		return (ret);
	/*
	 * !!!
	 * Since __db_mutex_init_int is a macro, we may not be
	 * using the 'offset' as it is only used for one type
	 * of mutex.  We COMPQUIET it here, after the call above.
	 */
	COMPQUIET(offset, 0);
	ret = __db_shreg_locks_record(dbenv, mutexp, infop, rp);

	/*
	 * If we couldn't record it and we are returning an error,
	 * we need to destroy the mutex we just created.
	 */
	if (ret)
		__db_mutex_destroy(mutexp);

	return (ret);
}

/*
 * __db_shreg_maintinit --
 *	Initialize a region's maintenance information.
 *
 * PUBLIC: void __db_shreg_maintinit __P((REGINFO *, void *addr, size_t));
 */
void
__db_shreg_maintinit(infop, addr, size)
	REGINFO *infop;
	void *addr;
	size_t size;
{
	REGMAINT *rp;
	u_int32_t i;

	rp = (REGMAINT *)addr;
	memset(addr, 0, sizeof(REGMAINT));
	rp->reglocks = size / sizeof(roff_t);
	rp->regmutex_hint = R_OFFSET(infop, &rp->regmutexes[0]);
	for (i = 0; i < rp->reglocks; i++)
		rp->regmutexes[i] = INVALID_ROFF;
}

static REGMAINT *
__db_mutex_maint(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	roff_t moff;

	switch (infop->type) {
	case REGION_TYPE_LOCK:
		moff = ((DB_LOCKREGION *)
		    R_ADDR(infop, infop->rp->primary))->maint_off;
		break;
	case REGION_TYPE_LOG:
		moff = ((LOG *)R_ADDR(infop, infop->rp->primary))->maint_off;
		break;
	case REGION_TYPE_MPOOL:
		moff = ((MPOOL *)R_ADDR(infop, infop->rp->primary))->maint_off;
		break;
	case REGION_TYPE_TXN:
		moff = ((DB_TXNREGION *)
		    R_ADDR(infop, infop->rp->primary))->maint_off;
		break;
	default:
		__db_err(dbenv,
	"Attempting to record mutex in a region not set up to do so");
		return (NULL);
	}
	return ((REGMAINT *)R_ADDR(infop, moff));
}
#endif /* HAVE_MUTEX_SYSTEM_RESOURCES */
