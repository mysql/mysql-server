/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_method.c,v 11.5 2000/12/21 19:16:42 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"

/*
 * __lock_set_lk_conflicts
 *	Set the conflicts matrix.
 *
 * PUBLIC: int __lock_set_lk_conflicts __P((DB_ENV *, u_int8_t *, int));
 */
int
__lock_set_lk_conflicts(dbenv, lk_conflicts, lk_modes)
	DB_ENV *dbenv;
	u_int8_t *lk_conflicts;
	int lk_modes;
{
	int ret;

	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_conflicts");

	if (dbenv->lk_conflicts != NULL) {
		__os_free(dbenv->lk_conflicts,
		    dbenv->lk_modes * dbenv->lk_modes);
		dbenv->lk_conflicts = NULL;
	}
	if ((ret = __os_malloc(dbenv,
	    lk_modes * lk_modes, NULL, &dbenv->lk_conflicts)) != 0)
		return (ret);
	memcpy(dbenv->lk_conflicts, lk_conflicts, lk_modes * lk_modes);
	dbenv->lk_modes = lk_modes;

	return (0);
}

/*
 * __lock_set_lk_detect
 *	Set the automatic deadlock detection.
 *
 * PUBLIC: int __lock_set_lk_detect __P((DB_ENV *, u_int32_t));
 */
int
__lock_set_lk_detect(dbenv, lk_detect)
	DB_ENV *dbenv;
	u_int32_t lk_detect;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_detect");

	switch (lk_detect) {
	case DB_LOCK_DEFAULT:
	case DB_LOCK_OLDEST:
	case DB_LOCK_RANDOM:
	case DB_LOCK_YOUNGEST:
		break;
	default:
		return (EINVAL);
	}
	dbenv->lk_detect = lk_detect;
	return (0);
}

/*
 * __lock_set_lk_max
 *	Set the lock table size.
 *
 * PUBLIC: int __lock_set_lk_max __P((DB_ENV *, u_int32_t));
 */
int
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
 *
 * PUBLIC: int __lock_set_lk_max_locks __P((DB_ENV *, u_int32_t));
 */
int
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
 *
 * PUBLIC: int __lock_set_lk_max_lockers __P((DB_ENV *, u_int32_t));
 */
int
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
 *
 * PUBLIC: int __lock_set_lk_max_objects __P((DB_ENV *, u_int32_t));
 */
int
__lock_set_lk_max_objects(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_lk_max_objects");

	dbenv->lk_max_objects = lk_max;
	return (0);
}
