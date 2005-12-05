/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_alloc.c,v 12.6 2005/08/08 14:57:54 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/mutex_int.h"

static int __mutex_free_int __P((DB_ENV *, int, db_mutex_t *));

/*
 * __mutex_alloc --
 *	Allocate a mutex from the mutex region.
 *
 * PUBLIC: int __mutex_alloc __P((DB_ENV *, int, u_int32_t, db_mutex_t *));
 */
int
__mutex_alloc(dbenv, alloc_id, flags, indxp)
	DB_ENV *dbenv;
	int alloc_id;
	u_int32_t flags;
	db_mutex_t *indxp;
{
	int ret;

	/* The caller may depend on us to initialize. */
	*indxp = MUTEX_INVALID;

	/*
	 * If this is not an application lock, and we've turned off locking,
	 * or the DB_ENV handle isn't thread-safe, and this is a thread lock
	 * or the environment isn't multi-process by definition, there's no
	 * need to mutex at all.
	 */
	if (alloc_id != MTX_APPLICATION &&
	    (F_ISSET(dbenv, DB_ENV_NOLOCKING) ||
	    (!F_ISSET(dbenv, DB_ENV_THREAD) &&
	    (LF_ISSET(DB_MUTEX_THREAD) || F_ISSET(dbenv, DB_ENV_PRIVATE)))))
		return (0);

	/*
	 * If we have a region in which to allocate the mutexes, lock it and
	 * do the allocation.
	 */
	if (MUTEX_ON(dbenv))
		return (__mutex_alloc_int(dbenv, 1, alloc_id, flags, indxp));

	/*
	 * We have to allocate some number of mutexes before we have a region
	 * in which to allocate them.  We handle this by saving up the list of
	 * flags and allocating them as soon as we have a handle.
	 *
	 * The list of mutexes to alloc is maintained in pairs: first the
	 * alloc_id argument, second the flags passed in by the caller.
	 */
	if (dbenv->mutex_iq == NULL) {
		dbenv->mutex_iq_max = 50;
		if ((ret = __os_calloc(dbenv, dbenv->mutex_iq_max,
		    sizeof(dbenv->mutex_iq[0]), &dbenv->mutex_iq)) != 0)
			return (ret);
	} else if (dbenv->mutex_iq_next == dbenv->mutex_iq_max - 1) {
		dbenv->mutex_iq_max *= 2;
		if ((ret = __os_realloc(dbenv,
		    dbenv->mutex_iq_max * sizeof(dbenv->mutex_iq[0]),
		    &dbenv->mutex_iq)) != 0)
			return (ret);
	}
	*indxp = dbenv->mutex_iq_next + 1;	/* Correct for MUTEX_INVALID. */
	dbenv->mutex_iq[dbenv->mutex_iq_next].alloc_id = alloc_id;
	dbenv->mutex_iq[dbenv->mutex_iq_next].flags = flags;
	++dbenv->mutex_iq_next;

	return (0);
}

/*
 * __mutex_alloc_int --
 *	Internal routine to allocate a mutex.
 *
 * PUBLIC: int __mutex_alloc_int
 * PUBLIC:	__P((DB_ENV *, int, int, u_int32_t, db_mutex_t *));
 */
int
__mutex_alloc_int(dbenv, locksys, alloc_id, flags, indxp)
	DB_ENV *dbenv;
	int locksys, alloc_id;
	u_int32_t flags;
	db_mutex_t *indxp;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int ret;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	ret = 0;

	/*
	 * If we're not initializing the mutex region, then lock the region to
	 * allocate new mutexes.  Drop the lock before initializing the mutex,
	 * mutex initialization may require a system call.
	 */
	if (locksys)
		MUTEX_SYSTEM_LOCK(dbenv);

	if (mtxregion->mutex_next == MUTEX_INVALID) {
		__db_err(dbenv,
		    "unable to allocate memory for mutex; resize mutex region");
		if (locksys)
			MUTEX_SYSTEM_UNLOCK(dbenv);
		return (ENOMEM);
	}

	*indxp = mtxregion->mutex_next;
	mutexp = MUTEXP_SET(*indxp);
	mtxregion->mutex_next = mutexp->mutex_next_link;

	--mtxregion->stat.st_mutex_free;
	++mtxregion->stat.st_mutex_inuse;
	if (mtxregion->stat.st_mutex_inuse > mtxregion->stat.st_mutex_inuse_max)
		mtxregion->stat.st_mutex_inuse_max =
		    mtxregion->stat.st_mutex_inuse;
	if (locksys)
		MUTEX_SYSTEM_UNLOCK(dbenv);

	/* Initialize the mutex. */
	memset(mutexp, 0, sizeof(*mutexp));

	F_SET(mutexp, DB_MUTEX_ALLOCATED);
	if (LF_ISSET(DB_MUTEX_LOGICAL_LOCK))
		F_SET(mutexp, DB_MUTEX_LOGICAL_LOCK);

#ifdef DIAGNOSTIC
	mutexp->alloc_id = alloc_id;
#else
	COMPQUIET(alloc_id, 0);
#endif

	if ((ret = __mutex_init(dbenv, *indxp, flags)) != 0)
		(void)__mutex_free_int(dbenv, locksys, indxp);

	return (ret);
}

/*
 * __mutex_free --
 *	Free a mutex.
 *
 * PUBLIC: int __mutex_free __P((DB_ENV *, db_mutex_t *));
 */
int
__mutex_free(dbenv, indxp)
	DB_ENV *dbenv;
	db_mutex_t *indxp;
{
	/*
	 * There is no explicit ordering in how the regions are cleaned up
	 * up and/or discarded when an environment is destroyed (either a
	 * private environment is closed or a public environment is removed).
	 * The way we deal with mutexes is to clean up all remaining mutexes
	 * when we close the mutex environment (because we have to be able to
	 * do that anyway, after a crash), which means we don't have to deal
	 * with region cleanup ordering on normal environment destruction.
	 * All that said, what it really means is we can get here without a
	 * mpool region.  It's OK, the mutex has been, or will be, destroyed.
	 *
	 * If the mutex has never been configured, we're done.
	 */
	if (!MUTEX_ON(dbenv) || *indxp == MUTEX_INVALID)
		return (0);

	return (__mutex_free_int(dbenv, 1, indxp));
}

/*
 * __mutex_free_int --
 *	Internal routine to free a mutex.
 */
static int
__mutex_free_int(dbenv, locksys, indxp)
	DB_ENV *dbenv;
	int locksys;
	db_mutex_t *indxp;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	db_mutex_t mutex;
	int ret;

	mutex = *indxp;
	*indxp = MUTEX_INVALID;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	DB_ASSERT(F_ISSET(mutexp, DB_MUTEX_ALLOCATED));
	F_CLR(mutexp, DB_MUTEX_ALLOCATED);

	ret = __mutex_destroy(dbenv, mutex);

	if (locksys)
		MUTEX_SYSTEM_LOCK(dbenv);

	/* Link the mutex on the head of the free list. */
	mutexp->mutex_next_link = mtxregion->mutex_next;
	mtxregion->mutex_next = mutex;
	++mtxregion->stat.st_mutex_free;
	--mtxregion->stat.st_mutex_inuse;

	if (locksys)
		MUTEX_SYSTEM_UNLOCK(dbenv);

	return (ret);
}
