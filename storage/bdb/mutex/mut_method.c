/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_method.c,v 12.5 2005/10/07 20:21:34 ubell Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/mutex_int.h"

/*
 * __mutex_alloc_pp --
 *	Allocate a mutex, application method.
 *
 * PUBLIC: int __mutex_alloc_pp __P((DB_ENV *, u_int32_t, db_mutex_t *));
 */
int
__mutex_alloc_pp(dbenv, flags, indxp)
	DB_ENV *dbenv;
	u_int32_t flags;
	db_mutex_t *indxp;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);

	if (flags != 0 && flags != DB_MUTEX_SELF_BLOCK)
		return (__db_ferr(dbenv, "DB_ENV->mutex_alloc", 0));

	ENV_ENTER(dbenv, ip);
	ret = __mutex_alloc(dbenv, MTX_APPLICATION, flags, indxp);
	ENV_LEAVE(dbenv, ip);

	return (ret);
}

/*
 * __mutex_free_pp --
 *	Destroy a mutex, application method.
 *
 * PUBLIC: int __mutex_free_pp __P((DB_ENV *, db_mutex_t));
 */
int
__mutex_free_pp(dbenv, indx)
	DB_ENV *dbenv;
	db_mutex_t indx;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);

	if (indx == MUTEX_INVALID)
		return (EINVAL);

	/*
	 * Internally Berkeley DB passes around the db_mutex_t address on
	 * free, because we want to make absolutely sure the slot gets
	 * overwritten with MUTEX_INVALID.  We don't export MUTEX_INVALID,
	 * so we don't export that part of the API, either.
	 */
	ENV_ENTER(dbenv, ip);
	ret = __mutex_free(dbenv, &indx);
	ENV_LEAVE(dbenv, ip);

	return (ret);
}

/*
 * __mutex_lock --
 *	Lock a mutex, application method.
 *
 * PUBLIC: int __mutex_lock_pp __P((DB_ENV *, db_mutex_t));
 */
int
__mutex_lock_pp(dbenv, indx)
	DB_ENV *dbenv;
	db_mutex_t indx;
{
	PANIC_CHECK(dbenv);

	if (indx == MUTEX_INVALID)
		return (EINVAL);

	return (__mutex_lock(dbenv, indx));
}

/*
 * __mutex_unlock --
 *	Unlock a mutex, application method.
 *
 * PUBLIC: int __mutex_unlock_pp __P((DB_ENV *, db_mutex_t));
 */
int
__mutex_unlock_pp(dbenv, indx)
	DB_ENV *dbenv;
	db_mutex_t indx;
{
	PANIC_CHECK(dbenv);

	if (indx == MUTEX_INVALID)
		return (EINVAL);

	return (__mutex_unlock(dbenv, indx));
}

/*
 * __mutex_get_align --
 *	DB_ENV->mutex_get_align.
 *
 * PUBLIC: int __mutex_get_align __P((DB_ENV *, u_int32_t *));
 */
int
__mutex_get_align(dbenv, alignp)
	DB_ENV *dbenv;
	u_int32_t *alignp;
{
	if (MUTEX_ON(dbenv))
		*alignp = ((DB_MUTEXREGION *)((DB_MUTEXMGR *)
		    dbenv->mutex_handle)->reginfo.primary)->stat.st_mutex_align;
	else
		*alignp = dbenv->mutex_align;
	return (0);
}

/*
 * __mutex_set_align --
 *	DB_ENV->mutex_set_align.
 *
 * PUBLIC: int __mutex_set_align __P((DB_ENV *, u_int32_t));
 */
int
__mutex_set_align(dbenv, align)
	DB_ENV *dbenv;
	u_int32_t align;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_mutex_align");

	if (align == 0 || !POWER_OF_TWO(align)) {
		__db_err(dbenv,
    "DB_ENV->mutex_set_align: alignment value must be a non-zero power-of-two");
		return (EINVAL);
	}

	dbenv->mutex_align = align;
	return (0);
}

/*
 * __mutex_get_increment --
 *	DB_ENV->mutex_get_increment.
 *
 * PUBLIC: int __mutex_get_increment __P((DB_ENV *, u_int32_t *));
 */
int
__mutex_get_increment(dbenv, incrementp)
	DB_ENV *dbenv;
	u_int32_t *incrementp;
{
	/*
	 * We don't maintain the increment in the region (it just makes
	 * no sense).  Return whatever we have configured on this handle,
	 * nobody is ever going to notice.
	 */
	*incrementp = dbenv->mutex_inc;
	return (0);
}

/*
 * __mutex_set_increment --
 *	DB_ENV->mutex_set_increment.
 *
 * PUBLIC: int __mutex_set_increment __P((DB_ENV *, u_int32_t));
 */
int
__mutex_set_increment(dbenv, increment)
	DB_ENV *dbenv;
	u_int32_t increment;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_mutex_increment");

	dbenv->mutex_cnt = 0;
	dbenv->mutex_inc = increment;
	return (0);
}

/*
 * __mutex_get_max --
 *	DB_ENV->mutex_get_max.
 *
 * PUBLIC: int __mutex_get_max __P((DB_ENV *, u_int32_t *));
 */
int
__mutex_get_max(dbenv, maxp)
	DB_ENV *dbenv;
	u_int32_t *maxp;
{
	if (MUTEX_ON(dbenv))
		*maxp = ((DB_MUTEXREGION *)((DB_MUTEXMGR *)
		    dbenv->mutex_handle)->reginfo.primary)->stat.st_mutex_cnt;
	else
		*maxp = dbenv->mutex_cnt;
	return (0);
}

/*
 * __mutex_set_max --
 *	DB_ENV->mutex_set_max.
 *
 * PUBLIC: int __mutex_set_max __P((DB_ENV *, u_int32_t));
 */
int
__mutex_set_max(dbenv, max)
	DB_ENV *dbenv;
	u_int32_t max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_mutex_max");

	dbenv->mutex_cnt = max;
	dbenv->mutex_inc = 0;
	return (0);
}

/*
 * __mutex_get_tas_spins --
 *	DB_ENV->mutex_get_tas_spins.
 *
 * PUBLIC: int __mutex_get_tas_spins __P((DB_ENV *, u_int32_t *));
 */
int
__mutex_get_tas_spins(dbenv, tas_spinsp)
	DB_ENV *dbenv;
	u_int32_t *tas_spinsp;
{
	if (MUTEX_ON(dbenv))
		*tas_spinsp = ((DB_MUTEXREGION *)((DB_MUTEXMGR *)dbenv->
		    mutex_handle)->reginfo.primary)->stat.st_mutex_tas_spins;
	else
		*tas_spinsp = dbenv->mutex_tas_spins;
	return (0);
}

/*
 * __mutex_set_tas_spins --
 *	DB_ENV->mutex_set_tas_spins.
 *
 * PUBLIC: int __mutex_set_tas_spins __P((DB_ENV *, u_int32_t));
 */
int
__mutex_set_tas_spins(dbenv, tas_spins)
	DB_ENV *dbenv;
	u_int32_t tas_spins;
{
	/*
	 * There's a theoretical race here, but I'm not interested in locking
	 * the test-and-set spin count.  The worst possibility is a thread
	 * reads out a bad spin count and spins until it gets the lock, but
	 * that's awfully unlikely.
	 */
	if (MUTEX_ON(dbenv))
		((DB_MUTEXREGION *)((DB_MUTEXMGR *)dbenv->mutex_handle)
		    ->reginfo.primary)->stat.st_mutex_tas_spins = tas_spins;
	else
		dbenv->mutex_tas_spins = tas_spins;
	return (0);
}
