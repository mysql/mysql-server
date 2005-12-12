/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_region.c,v 12.9 2005/10/27 15:16:13 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/log.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/mutex_int.h"

static int __mutex_region_init __P((DB_ENV *, DB_MUTEXMGR *));
static size_t __mutex_region_size __P((DB_ENV *));

/*
 * __mutex_open --
 *	Open a mutex region.
 *
 * PUBLIC: int __mutex_open __P((DB_ENV *));
 */
int
__mutex_open(dbenv)
	DB_ENV *dbenv;
{
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	db_mutex_t mutex;
	u_int i;
	int ret;

	/*
	 * Initialize the DB_ENV handle information if not already initialized.
	 *
	 * Align mutexes on the byte boundaries specified by the application.
	 */
	if (dbenv->mutex_align == 0)
		dbenv->mutex_align = MUTEX_ALIGN;
	if (dbenv->mutex_tas_spins == 0)
		dbenv->mutex_tas_spins = __os_spin(dbenv);

	/*
	 * If the user didn't set an absolute value on the number of mutexes
	 * we'll need, figure it out.  We're conservative in our allocation,
	 * we need mutexes for DB handles, group-commit queues and other things
	 * applications allocate at run-time.  The application may have kicked
	 * up our count to allocate its own mutexes, add that in.
	 */
	if (dbenv->mutex_cnt == 0)
		dbenv->mutex_cnt =
		    __lock_region_mutex_count(dbenv) +
		    __log_region_mutex_count(dbenv) +
		    __memp_region_mutex_count(dbenv) +
		    dbenv->mutex_inc + 500;

	/* Create/initialize the mutex manager structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_MUTEXMGR), &mtxmgr)) != 0)
		return (ret);

	/* Join/create the txn region. */
	mtxmgr->reginfo.dbenv = dbenv;
	mtxmgr->reginfo.type = REGION_TYPE_MUTEX;
	mtxmgr->reginfo.id = INVALID_REGION_ID;
	mtxmgr->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&mtxmgr->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(dbenv,
	    &mtxmgr->reginfo, __mutex_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&mtxmgr->reginfo, REGION_CREATE))
		if ((ret = __mutex_region_init(dbenv, mtxmgr)) != 0)
			goto err;

	/* Set the local addresses. */
	mtxregion = mtxmgr->reginfo.primary =
	    R_ADDR(&mtxmgr->reginfo, mtxmgr->reginfo.rp->primary);
	mtxmgr->mutex_array = R_ADDR(&mtxmgr->reginfo, mtxregion->mutex_offset);

	dbenv->mutex_handle = mtxmgr;

	/* Allocate initial queue of mutexes. */
	if (dbenv->mutex_iq != NULL) {
		DB_ASSERT(F_ISSET(&mtxmgr->reginfo, REGION_CREATE));
		for (i = 0; i < dbenv->mutex_iq_next; ++i) {
			if ((ret = __mutex_alloc_int(
			    dbenv, 0, dbenv->mutex_iq[i].alloc_id,
			    dbenv->mutex_iq[i].flags, &mutex)) != 0)
				goto err;
			/*
			 * Confirm we allocated the right index, correcting
			 * for avoiding slot 0 (MUTEX_INVALID).
			 */
			DB_ASSERT(mutex == i + 1);
		}
		__os_free(dbenv, dbenv->mutex_iq);
		dbenv->mutex_iq = NULL;

		/*
		 * This is the first place we can test mutexes and we need to
		 * know if they're working.  (They CAN fail, for example on
		 * SunOS, when using fcntl(2) for locking and using an
		 * in-memory filesystem as the database environment directory.
		 * But you knew that, I'm sure -- it probably wasn't worth
		 * mentioning.)
		 */
		mutex = MUTEX_INVALID;
		if ((ret =
		    __mutex_alloc(dbenv, MTX_MUTEX_TEST, 0, &mutex) != 0) ||
		    (ret = __mutex_lock(dbenv, mutex)) != 0 ||
		    (ret = __mutex_unlock(dbenv, mutex)) != 0 ||
		    (ret = __mutex_free(dbenv, &mutex)) != 0) {
			__db_err(dbenv,
		    "Unable to acquire/release a mutex; check configuration");
			goto err;
		}
	}

	/*
	 * Initialize thread tracking.  We want to do this as early
	 * has possible in case we die.  This sits in the mutex region
	 * so do it now.
	 */
	if ((ret = __env_thread_init(dbenv,
	    F_ISSET(&mtxmgr->reginfo, REGION_CREATE))) != 0)
		goto err;

	return (0);

err:	dbenv->mutex_handle = NULL;
	if (mtxmgr->reginfo.addr != NULL)
		(void)__db_r_detach(dbenv, &mtxmgr->reginfo, 0);

	__os_free(dbenv, mtxmgr);
	return (ret);
}

/*
 * __mutex_region_init --
 *	Initialize a mutex region in shared memory.
 */
static int
__mutex_region_init(dbenv, mtxmgr)
	DB_ENV *dbenv;
	DB_MUTEXMGR *mtxmgr;
{
	DB_MUTEXREGION *mtxregion;
	DB_MUTEX *mutexp;
	db_mutex_t i;
	int ret;
	void *mutex_array;

	COMPQUIET(mutexp, NULL);

	if ((ret = __db_shalloc(&mtxmgr->reginfo,
	    sizeof(DB_MUTEXREGION), 0, &mtxmgr->reginfo.primary)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for the mutex region");
		return (ret);
	}
	mtxmgr->reginfo.rp->primary =
	    R_OFFSET(&mtxmgr->reginfo, mtxmgr->reginfo.primary);
	mtxregion = mtxmgr->reginfo.primary;
	memset(mtxregion, 0, sizeof(*mtxregion));

	if ((ret = __mutex_alloc(
	    dbenv, MTX_MUTEX_REGION, 0, &mtxregion->mtx_region)) != 0)
		return (ret);

	mtxregion->mutex_size =
	    (size_t)DB_ALIGN(sizeof(DB_MUTEX), dbenv->mutex_align);

	mtxregion->stat.st_mutex_align = dbenv->mutex_align;
	mtxregion->stat.st_mutex_cnt = dbenv->mutex_cnt;
	mtxregion->stat.st_mutex_tas_spins = dbenv->mutex_tas_spins;

	/*
	 * Get a chunk of memory to be used for the mutexes themselves.  Each
	 * piece of the memory must be properly aligned.
	 *
	 * The OOB mutex (MUTEX_INVALID) is 0.  To make this work, we ignore
	 * the first allocated slot when we build the free list.  We have to
	 * correct the count by 1 here, though, otherwise our counter will be
	 * off by 1.
	 */
	if ((ret = __db_shalloc(&mtxmgr->reginfo,
	    (mtxregion->stat.st_mutex_cnt + 1) * mtxregion->mutex_size,
	    mtxregion->stat.st_mutex_align, &mutex_array)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for mutexes from the region");
		return (ret);
	}

	mtxregion->mutex_offset = R_OFFSET(&mtxmgr->reginfo, mutex_array);
	mtxmgr->mutex_array = mutex_array;

	/*
	 * Put the mutexes on a free list and clear the allocated flag.
	 *
	 * The OOB mutex (MUTEX_INVALID) is 0, skip it.
	 *
	 * The comparison is <, not <=, because we're looking ahead one
	 * in each link.
	 */
	for (i = 1; i < mtxregion->stat.st_mutex_cnt; ++i) {
		mutexp = MUTEXP_SET(i);
		mutexp->flags = 0;
		mutexp->mutex_next_link = i + 1;
	}
	mutexp = MUTEXP_SET(i);
	mutexp->flags = 0;
	mutexp->mutex_next_link = MUTEX_INVALID;
	mtxregion->mutex_next = 1;
	mtxregion->stat.st_mutex_free = mtxregion->stat.st_mutex_cnt;
	mtxregion->stat.st_mutex_inuse = mtxregion->stat.st_mutex_inuse_max = 0;

	return (0);
}

/*
 * __mutex_dbenv_refresh --
 *	Clean up after the mutex region on a close or failed open.
 *
 * PUBLIC: int __mutex_dbenv_refresh __P((DB_ENV *));
 */
int
__mutex_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	REGINFO *reginfo;
	int ret;

	mtxmgr = dbenv->mutex_handle;
	reginfo = &mtxmgr->reginfo;
	mtxregion = mtxmgr->reginfo.primary;

	/*
	 * If a private region, return the memory to the heap.  Not needed for
	 * filesystem-backed or system shared memory regions, that memory isn't
	 * owned by any particular process.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
		/*
		 * If destroying the mutex region, return any system resources
		 * to the system.
		 */
		__mutex_resource_return(dbenv, reginfo);
#endif
		/* Discard the mutex array. */
		__db_shalloc_free(
		    reginfo, R_ADDR(reginfo, mtxregion->mutex_offset));
	}

	/* Detach from the region. */
	ret = __db_r_detach(dbenv, reginfo, 0);

	__os_free(dbenv, mtxmgr);

	dbenv->mutex_handle = NULL;

	return (ret);
}

/*
 * __mutex_region_size --
 *	 Return the amount of space needed for the mutex region.
 */
static size_t
__mutex_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	s = sizeof(DB_MUTEXMGR) + 1024;
	s += dbenv->mutex_cnt *
	    __db_shalloc_size(sizeof(DB_MUTEX), dbenv->mutex_align);
	/*
	 * Allocate space for thread info blocks.  Max is only advisory,
	 * so we allocate 25% more.
	 */
	s += (dbenv->thr_max + dbenv->thr_max/4) *
	    __db_shalloc_size(sizeof(DB_THREAD_INFO), sizeof(roff_t));
	s += dbenv->thr_nbucket *
	    __db_shalloc_size(sizeof(DB_HASHTAB), sizeof(roff_t));
	return (s);
}

#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
/*
 * __mutex_resource_return
 *	Return any system-allocated mutex resources to the system.
 *
 * PUBLIC: void __mutex_resource_return __P((DB_ENV *, REGINFO *));
 */
void
__mutex_resource_return(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr, mtxmgr_st;
	DB_MUTEXREGION *mtxregion;
	db_mutex_t i;
	void *orig_handle;

	/*
	 * This routine is called in two cases: when discarding the regions
	 * from a previous Berkeley DB run, during recovery, and two, when
	 * discarding regions as we shut down the database environment.
	 *
	 * Walk the list of mutexes and destroy any live ones.
	 *
	 * This is just like joining a region -- the REGINFO we're handed
	 * is the same as the one returned by __db_r_attach(), all we have
	 * to do is fill in the links.
	 *
	 * !!!
	 * The region may be corrupted, of course.  We're safe because the
	 * only things we look at are things that are initialized when the
	 * region is created, and never modified after that.
	 */
	memset(&mtxmgr_st, 0, sizeof(mtxmgr_st));
	mtxmgr = &mtxmgr_st;
	mtxmgr->reginfo = *infop;
	mtxregion = mtxmgr->reginfo.primary =
	    R_ADDR(&mtxmgr->reginfo, mtxmgr->reginfo.rp->primary);
	mtxmgr->mutex_array = R_ADDR(&mtxmgr->reginfo, mtxregion->mutex_offset);

	/*
	 * This is a little strange, but the mutex_handle is what all of the
	 * underlying mutex routines will use to determine if they should do
	 * any work and to find their information.  Save/restore the handle
	 * around the work loop.
	 *
	 * The OOB mutex (MUTEX_INVALID) is 0, skip it.
	 */
	orig_handle = dbenv->mutex_handle;
	dbenv->mutex_handle = mtxmgr;
	for (i = 1; i <= mtxregion->stat.st_mutex_cnt; ++i, ++mutexp) {
		mutexp = MUTEXP_SET(i);
		if (F_ISSET(mutexp, DB_MUTEX_ALLOCATED))
			(void)__mutex_destroy(dbenv, i);
	}
	dbenv->mutex_handle = orig_handle;
}
#endif
