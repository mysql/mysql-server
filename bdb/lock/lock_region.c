/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_region.c,v 11.41 2000/12/20 21:53:04 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef	HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"

#ifdef	HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int  __lock_init __P((DB_ENV *, DB_LOCKTAB *));
static size_t
	    __lock_region_size __P((DB_ENV *));

#ifdef MUTEX_SYSTEM_RESOURCES
static size_t __lock_region_maint __P((DB_ENV *));
#endif

/*
 * This conflict array is used for concurrent db access (CDB).  It
 * uses the same locks as the db_rw_conflict array, but adds an IW
 * mode to be used for write cursors.
 */
#define	DB_LOCK_CDB_N 5
static u_int8_t const db_cdb_conflicts[] = {
	/*		N   R   W  WT  IW*/
	/*    N */	0,  0,  0,  0, 0,
	/*    R */	0,  0,  1,  0, 0,
	/*    W */	0,  1,  1,  1, 1,
	/*   WT */	0,  0,  0,  0, 0,
	/*   IW */	0,  0,  1,  0, 1,
};

/*
 * __lock_dbenv_create --
 *	Lock specific creation of the DB_ENV structure.
 *
 * PUBLIC: void __lock_dbenv_create __P((DB_ENV *));
 */
void
__lock_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	dbenv->lk_max = DB_LOCK_DEFAULT_N;
	dbenv->lk_max_lockers = DB_LOCK_DEFAULT_N;
	dbenv->lk_max_objects = DB_LOCK_DEFAULT_N;

	dbenv->set_lk_conflicts = __lock_set_lk_conflicts;
	dbenv->set_lk_detect = __lock_set_lk_detect;
	dbenv->set_lk_max = __lock_set_lk_max;
	dbenv->set_lk_max_locks = __lock_set_lk_max_locks;
	dbenv->set_lk_max_lockers = __lock_set_lk_max_lockers;
	dbenv->set_lk_max_objects = __lock_set_lk_max_objects;

#ifdef	HAVE_RPC
	/*
	 * If we have a client, overwrite what we just set up to point
	 * to the client functions.
	 */
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_lk_conflicts = __dbcl_set_lk_conflict;
		dbenv->set_lk_detect = __dbcl_set_lk_detect;
		dbenv->set_lk_max = __dbcl_set_lk_max;
		dbenv->set_lk_max_locks = __dbcl_set_lk_max_locks;
		dbenv->set_lk_max_lockers = __dbcl_set_lk_max_lockers;
		dbenv->set_lk_max_objects = __dbcl_set_lk_max_objects;
	}
#endif
}

/*
 * __lock_dbenv_close --
 *	Lock specific destruction of the DB_ENV structure.
 *
 * PUBLIC: void __lock_dbenv_close __P((DB_ENV *));
 */
void
__lock_dbenv_close(dbenv)
	DB_ENV *dbenv;
{
	if (!F_ISSET(dbenv, DB_ENV_USER_ALLOC) && dbenv->lk_conflicts != NULL) {
		__os_free(dbenv->lk_conflicts,
		    dbenv->lk_modes * dbenv->lk_modes);
		dbenv->lk_conflicts = NULL;
	}
}

/*
 * __lock_open --
 *	Internal version of lock_open: only called from DB_ENV->open.
 *
 * PUBLIC: int __lock_open __P((DB_ENV *));
 */
int
__lock_open(dbenv)
	DB_ENV *dbenv;
{
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	size_t size;
	int ret;

	/* Create the lock table structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOCKTAB), &lt)) != 0)
		return (ret);
	lt->dbenv = dbenv;

	/* Join/create the lock region. */
	lt->reginfo.type = REGION_TYPE_LOCK;
	lt->reginfo.id = INVALID_REGION_ID;
	lt->reginfo.mode = dbenv->db_mode;
	lt->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&lt->reginfo, REGION_CREATE_OK);
	size = __lock_region_size(dbenv);
	if ((ret = __db_r_attach(dbenv, &lt->reginfo, size)) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&lt->reginfo, REGION_CREATE))
		if ((ret = __lock_init(dbenv, lt)) != 0)
			goto err;

	/* Set the local addresses. */
	region = lt->reginfo.primary =
	    R_ADDR(&lt->reginfo, lt->reginfo.rp->primary);

	/* Check for incompatible automatic deadlock detection requests. */
	if (dbenv->lk_detect != DB_LOCK_NORUN) {
		if (region->detect != DB_LOCK_NORUN &&
		    dbenv->lk_detect != DB_LOCK_DEFAULT &&
		    region->detect != dbenv->lk_detect) {
			__db_err(dbenv,
		    "lock_open: incompatible deadlock detector mode");
			ret = EINVAL;
			goto err;
		}

		/*
		 * Upgrade if our caller wants automatic detection, and it
		 * was not currently being done, whether or not we created
		 * the region.
		 */
		if (region->detect == DB_LOCK_NORUN)
			region->detect = dbenv->lk_detect;
	}

	/* Set remaining pointers into region. */
	lt->conflicts = (u_int8_t *)R_ADDR(&lt->reginfo, region->conf_off);
	lt->obj_tab = (DB_HASHTAB *)R_ADDR(&lt->reginfo, region->obj_off);
	lt->locker_tab = (DB_HASHTAB *)R_ADDR(&lt->reginfo, region->locker_off);

	R_UNLOCK(dbenv, &lt->reginfo);

	dbenv->lk_handle = lt;
	return (0);

err:	if (lt->reginfo.addr != NULL) {
		if (F_ISSET(&lt->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);
		R_UNLOCK(dbenv, &lt->reginfo);
		(void)__db_r_detach(dbenv, &lt->reginfo, 0);
	}
	__os_free(lt, sizeof(*lt));
	return (ret);
}

/*
 * __lock_init --
 *	Initialize the lock region.
 */
static int
__lock_init(dbenv, lt)
	DB_ENV *dbenv;
	DB_LOCKTAB *lt;
{
	const u_int8_t *lk_conflicts;
	struct __db_lock *lp;
	DB_LOCKER *lidp;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *region;
#ifdef MUTEX_SYSTEM_RESOURCES
	size_t maint_size;
#endif
	u_int32_t i, lk_modes;
	u_int8_t *addr;
	int ret;

	if ((ret = __db_shalloc(lt->reginfo.addr,
	    sizeof(DB_LOCKREGION), 0, &lt->reginfo.primary)) != 0)
		goto mem_err;
	lt->reginfo.rp->primary = R_OFFSET(&lt->reginfo, lt->reginfo.primary);
	region = lt->reginfo.primary;
	memset(region, 0, sizeof(*region));

	/* Select a conflict matrix if none specified. */
	if (dbenv->lk_modes == 0)
		if (CDB_LOCKING(dbenv)) {
			lk_modes = DB_LOCK_CDB_N;
			lk_conflicts = db_cdb_conflicts;
		} else {
			lk_modes = DB_LOCK_RIW_N;
			lk_conflicts = db_riw_conflicts;
		}
	else {
		lk_modes = dbenv->lk_modes;
		lk_conflicts = dbenv->lk_conflicts;
	}

	region->id = 0;
	region->need_dd = 0;
	region->detect = DB_LOCK_NORUN;
	region->maxlocks = dbenv->lk_max;
	region->maxlockers = dbenv->lk_max_lockers;
	region->maxobjects = dbenv->lk_max_objects;
	region->locker_t_size = __db_tablesize(dbenv->lk_max_lockers);
	region->object_t_size = __db_tablesize(dbenv->lk_max_objects);
	region->nmodes = lk_modes;
	region->nlocks = 0;
	region->maxnlocks = 0;
	region->nlockers = 0;
	region->maxnlockers = 0;
	region->nobjects = 0;
	region->maxnobjects = 0;
	region->nconflicts = 0;
	region->nrequests = 0;
	region->nreleases = 0;
	region->ndeadlocks = 0;

	/* Allocate room for the conflict matrix and initialize it. */
	if ((ret =
	    __db_shalloc(lt->reginfo.addr, lk_modes * lk_modes, 0, &addr)) != 0)
		goto mem_err;
	memcpy(addr, lk_conflicts, lk_modes * lk_modes);
	region->conf_off = R_OFFSET(&lt->reginfo, addr);

	/* Allocate room for the object hash table and initialize it. */
	if ((ret = __db_shalloc(lt->reginfo.addr,
	    region->object_t_size * sizeof(DB_HASHTAB), 0, &addr)) != 0)
		goto mem_err;
	__db_hashinit(addr, region->object_t_size);
	region->obj_off = R_OFFSET(&lt->reginfo, addr);

	/* Allocate room for the locker hash table and initialize it. */
	if ((ret = __db_shalloc(lt->reginfo.addr,
	    region->locker_t_size * sizeof(DB_HASHTAB), 0, &addr)) != 0)
		goto mem_err;
	__db_hashinit(addr, region->locker_t_size);
	region->locker_off = R_OFFSET(&lt->reginfo, addr);

#ifdef	MUTEX_SYSTEM_RESOURCES
	maint_size = __lock_region_maint(dbenv);
	/* Allocate room for the locker maintenance info and initialize it. */
	if ((ret = __db_shalloc(lt->reginfo.addr,
	    sizeof(REGMAINT) + maint_size, 0, &addr)) != 0)
		goto mem_err;
	__db_maintinit(&lt->reginfo, addr, maint_size);
	region->maint_off = R_OFFSET(&lt->reginfo, addr);
#endif

	/*
	 * Initialize locks onto a free list. Initialize and lock the mutex
	 * so that when we need to block, all we need do is try to acquire
	 * the mutex.
	 */
	SH_TAILQ_INIT(&region->free_locks);
	for (i = 0; i < region->maxlocks; ++i) {
		if ((ret = __db_shalloc(lt->reginfo.addr,
		    sizeof(struct __db_lock), MUTEX_ALIGN, &lp)) != 0)
			goto mem_err;
		lp->status = DB_LSTAT_FREE;
		lp->gen=0;
		if ((ret = __db_shmutex_init(dbenv, &lp->mutex,
		    R_OFFSET(&lt->reginfo, &lp->mutex) + DB_FCNTL_OFF_LOCK,
		    MUTEX_SELF_BLOCK, &lt->reginfo,
		    (REGMAINT *)R_ADDR(&lt->reginfo, region->maint_off))) != 0)
			return (ret);
		MUTEX_LOCK(dbenv, &lp->mutex, lt->dbenv->lockfhp);
		SH_TAILQ_INSERT_HEAD(&region->free_locks, lp, links, __db_lock);
	}

	/* Initialize objects onto a free list.  */
	SH_TAILQ_INIT(&region->dd_objs);
	SH_TAILQ_INIT(&region->free_objs);
	for (i = 0; i < region->maxobjects; ++i) {
		if ((ret = __db_shalloc(lt->reginfo.addr,
		    sizeof(DB_LOCKOBJ), 0, &op)) != 0)
			goto mem_err;
		SH_TAILQ_INSERT_HEAD(
		    &region->free_objs, op, links, __db_lockobj);
	}

	/* Initialize lockers onto a free list.  */
	SH_TAILQ_INIT(&region->free_lockers);
	for (i = 0; i < region->maxlockers; ++i) {
		if ((ret = __db_shalloc(lt->reginfo.addr,
		    sizeof(DB_LOCKER), 0, &lidp)) != 0) {
mem_err:	__db_err(dbenv, "Unable to allocate memory for the lock table");
		return (ret);
	}
		SH_TAILQ_INSERT_HEAD(
		    &region->free_lockers, lidp, links, __db_locker);
	}

	return (0);
}

/*
 * __lock_close --
 *	Internal version of lock_close: only called from db_appinit.
 *
 * PUBLIC: int __lock_close __P((DB_ENV *));
 */
int
__lock_close(dbenv)
	DB_ENV *dbenv;
{
	DB_LOCKTAB *lt;
	int ret;

	lt = dbenv->lk_handle;

	/* Detach from the region. */
	ret = __db_r_detach(dbenv, &lt->reginfo, 0);

	__os_free(lt, sizeof(*lt));

	dbenv->lk_handle = NULL;
	return (ret);
}

/*
 * __lock_region_size --
 *	Return the region size.
 */
static size_t
__lock_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t retval;

	/*
	 * Figure out how much space we're going to need.  This list should
	 * map one-to-one with the __db_shalloc calls in __lock_init.
	 */
	retval = 0;
	retval += __db_shalloc_size(sizeof(DB_LOCKREGION), 1);
	retval += __db_shalloc_size(dbenv->lk_modes * dbenv->lk_modes, 1);
	retval += __db_shalloc_size(
	     __db_tablesize(dbenv->lk_max_lockers) * (sizeof(DB_HASHTAB)), 1);
	retval += __db_shalloc_size(
	     __db_tablesize(dbenv->lk_max_objects) * (sizeof(DB_HASHTAB)), 1);
#ifdef MUTEX_SYSTEM_RESOURCES
	retval +=
	    __db_shalloc_size(sizeof(REGMAINT) + __lock_region_maint(dbenv), 1);
#endif
	retval += __db_shalloc_size(
	     sizeof(struct __db_lock), MUTEX_ALIGN) * dbenv->lk_max;
	retval += __db_shalloc_size(sizeof(DB_LOCKOBJ), 1) * dbenv->lk_max_objects;
	retval += __db_shalloc_size(sizeof(DB_LOCKER), 1) * dbenv->lk_max_lockers;

	/*
	 * Include 16 bytes of string space per lock.  DB doesn't use it
	 * because we pre-allocate lock space for DBTs in the structure.
	 */
	retval += __db_shalloc_size(dbenv->lk_max * 16, sizeof(size_t));

	/* And we keep getting this wrong, let's be generous. */
	retval += retval / 4;

	return (retval);
}

#ifdef MUTEX_SYSTEM_RESOURCES
/*
 * __lock_region_maint --
 *	Return the amount of space needed for region maintenance info.
 */
static size_t
__lock_region_maint(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	s = sizeof(MUTEX *) * dbenv->lk_max;
	return (s);
}
#endif

/*
 * __lock_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __lock_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__lock_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	DB_LOCKREGION *region;

	COMPQUIET(dbenv, NULL);
	region = R_ADDR(infop, infop->rp->primary);

	__db_shlocks_destroy(infop,
	    (REGMAINT *)R_ADDR(infop, region->maint_off));
	return;
}
