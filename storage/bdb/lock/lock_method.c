/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_method.c,v 11.30 2002/03/27 04:32:20 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"

#ifdef HAVE_RPC
#include "dbinc_auto/db_server.h"
#include "dbinc_auto/rpc_client_ext.h"
#endif

static int __lock_set_lk_conflicts __P((DB_ENV *, u_int8_t *, int));
static int __lock_set_lk_detect __P((DB_ENV *, u_int32_t));
static int __lock_set_lk_max __P((DB_ENV *, u_int32_t));
static int __lock_set_lk_max_lockers __P((DB_ENV *, u_int32_t));
static int __lock_set_lk_max_locks __P((DB_ENV *, u_int32_t));
static int __lock_set_lk_max_objects __P((DB_ENV *, u_int32_t));
static int __lock_set_env_timeout __P((DB_ENV *, db_timeout_t, u_int32_t));

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
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 */

	dbenv->lk_max = DB_LOCK_DEFAULT_N;
	dbenv->lk_max_lockers = DB_LOCK_DEFAULT_N;
	dbenv->lk_max_objects = DB_LOCK_DEFAULT_N;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_lk_conflicts = __dbcl_set_lk_conflict;
		dbenv->set_lk_detect = __dbcl_set_lk_detect;
		dbenv->set_lk_max = __dbcl_set_lk_max;
		dbenv->set_lk_max_lockers = __dbcl_set_lk_max_lockers;
		dbenv->set_lk_max_locks = __dbcl_set_lk_max_locks;
		dbenv->set_lk_max_objects = __dbcl_set_lk_max_objects;
		dbenv->lock_detect = __dbcl_lock_detect;
		dbenv->lock_dump_region = NULL;
		dbenv->lock_get = __dbcl_lock_get;
		dbenv->lock_id = __dbcl_lock_id;
		dbenv->lock_id_free = __dbcl_lock_id_free;
		dbenv->lock_put = __dbcl_lock_put;
		dbenv->lock_stat = __dbcl_lock_stat;
		dbenv->lock_vec = __dbcl_lock_vec;
	} else
#endif
	{
		dbenv->set_lk_conflicts = __lock_set_lk_conflicts;
		dbenv->set_lk_detect = __lock_set_lk_detect;
		dbenv->set_lk_max = __lock_set_lk_max;
		dbenv->set_lk_max_lockers = __lock_set_lk_max_lockers;
		dbenv->set_lk_max_locks = __lock_set_lk_max_locks;
		dbenv->set_lk_max_objects = __lock_set_lk_max_objects;
		dbenv->set_timeout = __lock_set_env_timeout;
		dbenv->lock_detect = __lock_detect;
		dbenv->lock_dump_region = __lock_dump_region;
		dbenv->lock_get = __lock_get;
		dbenv->lock_id = __lock_id;
		dbenv->lock_id_free = __lock_id_free;
#ifdef CONFIG_TEST
		dbenv->lock_id_set = __lock_id_set;
#endif
		dbenv->lock_put = __lock_put;
		dbenv->lock_stat = __lock_stat;
		dbenv->lock_vec = __lock_vec;
		dbenv->lock_downgrade = __lock_downgrade;
	}
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
	if (dbenv->lk_conflicts != NULL) {
		__os_free(dbenv, dbenv->lk_conflicts);
		dbenv->lk_conflicts = NULL;
	}
}

/*
 * __lock_set_lk_conflicts
 *	Set the conflicts matrix.
 */
static int
__lock_set_lk_conflicts(dbenv, lk_conflicts, lk_modes)
	DB_ENV *dbenv;
	u_int8_t *lk_conflicts;
	int lk_modes;
{
	int ret;

	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_conflicts");

	if (dbenv->lk_conflicts != NULL) {
		__os_free(dbenv, dbenv->lk_conflicts);
		dbenv->lk_conflicts = NULL;
	}
	if ((ret = __os_malloc(dbenv,
	    lk_modes * lk_modes, &dbenv->lk_conflicts)) != 0)
		return (ret);
	memcpy(dbenv->lk_conflicts, lk_conflicts, lk_modes * lk_modes);
	dbenv->lk_modes = lk_modes;

	return (0);
}

/*
 * __lock_set_lk_detect
 *	Set the automatic deadlock detection.
 */
static int
__lock_set_lk_detect(dbenv, lk_detect)
	DB_ENV *dbenv;
	u_int32_t lk_detect;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_detect");

	switch (lk_detect) {
	case DB_LOCK_DEFAULT:
	case DB_LOCK_EXPIRE:
	case DB_LOCK_MAXLOCKS:
	case DB_LOCK_MINLOCKS:
	case DB_LOCK_MINWRITE:
	case DB_LOCK_OLDEST:
	case DB_LOCK_RANDOM:
	case DB_LOCK_YOUNGEST:
		break;
	default:
		__db_err(dbenv,
	    "DB_ENV->set_lk_detect: unknown deadlock detection mode specified");
		return (EINVAL);
	}
	dbenv->lk_detect = lk_detect;
	return (0);
}

/*
 * __lock_set_lk_max
 *	Set the lock table size.
 */
static int
__lock_set_lk_max(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_max");

	dbenv->lk_max = lk_max;
	dbenv->lk_max_objects = lk_max;
	dbenv->lk_max_lockers = lk_max;
	return (0);
}

/*
 * __lock_set_lk_max_locks
 *	Set the lock table size.
 */
static int
__lock_set_lk_max_locks(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_max_locks");

	dbenv->lk_max = lk_max;
	return (0);
}

/*
 * __lock_set_lk_max_lockers
 *	Set the lock table size.
 */
static int
__lock_set_lk_max_lockers(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_max_lockers");

	dbenv->lk_max_lockers = lk_max;
	return (0);
}

/*
 * __lock_set_lk_max_objects
 *	Set the lock table size.
 */
static int
__lock_set_lk_max_objects(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_max_objects");

	dbenv->lk_max_objects = lk_max;
	return (0);
}

/*
 * __lock_set_env_timeout
 *	Set the lock environment timeout.
 */
static int
__lock_set_env_timeout(dbenv, timeout, flags)
	DB_ENV *dbenv;
	db_timeout_t timeout;
	u_int32_t flags;
{
	DB_LOCKREGION *region;

	region = NULL;
	if (F_ISSET(dbenv, DB_ENV_OPEN_CALLED)) {
		if (!LOCKING_ON(dbenv))
			return (__db_env_config(
			    dbenv, "set_timeout", DB_INIT_LOCK));
		region = ((DB_LOCKTAB *)dbenv->lk_handle)->reginfo.primary;
	}

	switch (flags) {
	case DB_SET_LOCK_TIMEOUT:
		dbenv->lk_timeout = timeout;
		if (region != NULL)
			region->lk_timeout = timeout;
		break;
	case DB_SET_TXN_TIMEOUT:
		dbenv->tx_timeout = timeout;
		if (region != NULL)
			region->tx_timeout = timeout;
		break;
	default:
		return (__db_ferr(dbenv, "DB_ENV->set_timeout", 0));
		/* NOTREACHED */
	}

	return (0);
}
