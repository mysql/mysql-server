/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: lock_timer.c,v 11.141 2004/03/24 20:51:39 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <stdlib.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"

/*
 * __lock_set_timeout
 *		-- set timeout values in shared memory.
 *
 * This is called from the transaction system.  We either set the time that
 * this transaction expires or the amount of time a lock for this transaction
 * is permitted to wait.
 *
 * PUBLIC: int __lock_set_timeout __P(( DB_ENV *,
 * PUBLIC:      u_int32_t, db_timeout_t, u_int32_t));
 */
int
__lock_set_timeout(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	u_int32_t locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	DB_LOCKTAB *lt;
	int ret;

	lt = dbenv->lk_handle;

	LOCKREGION(dbenv, lt);
	ret = __lock_set_timeout_internal(dbenv, locker, timeout, op);
	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_set_timeout_internal
 *		-- set timeout values in shared memory.
 *
 * This is the internal version called from the lock system.  We either set
 * the time that this transaction expires or the amount of time that a lock
 * for this transaction is permitted to wait.
 *
 * PUBLIC: int __lock_set_timeout_internal
 * PUBLIC:     __P((DB_ENV *, u_int32_t, db_timeout_t, u_int32_t));
 */
int
__lock_set_timeout_internal(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	u_int32_t locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t locker_ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKER_LOCK(lt, region, locker, locker_ndx);
	ret = __lock_getlocker(lt, locker, locker_ndx, 1, &sh_locker);

	if (ret != 0)
		return (ret);

	if (op == DB_SET_TXN_TIMEOUT) {
		if (timeout == 0)
			LOCK_SET_TIME_INVALID(&sh_locker->tx_expire);
		else
			__lock_expires(dbenv, &sh_locker->tx_expire, timeout);
	} else if (op == DB_SET_LOCK_TIMEOUT) {
		sh_locker->lk_timeout = timeout;
		F_SET(sh_locker, DB_LOCKER_TIMEOUT);
	} else if (op == DB_SET_TXN_NOW) {
		LOCK_SET_TIME_INVALID(&sh_locker->tx_expire);
		__lock_expires(dbenv, &sh_locker->tx_expire, 0);
		sh_locker->lk_expire = sh_locker->tx_expire;
		if (!LOCK_TIME_ISVALID(&region->next_timeout) ||
		    LOCK_TIME_GREATER(
		    &region->next_timeout, &sh_locker->lk_expire))
			region->next_timeout = sh_locker->lk_expire;
	} else
		return (EINVAL);

	return (0);
}

/*
 * __lock_inherit_timeout
 *		-- inherit timeout values from parent locker.
 * This is called from the transaction system.  This will
 * return EINVAL if the parent does not exist or did not
 * have a current txn timeout set.
 *
 * PUBLIC: int __lock_inherit_timeout __P(( DB_ENV *, u_int32_t, u_int32_t));
 */
int
__lock_inherit_timeout(dbenv, parent, locker)
	DB_ENV *dbenv;
	u_int32_t parent, locker;
{
	DB_LOCKER *parent_locker, *sh_locker;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t locker_ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	ret = 0;
	LOCKREGION(dbenv, lt);

	/* If the parent does not exist, we are done. */
	LOCKER_LOCK(lt, region, parent, locker_ndx);
	if ((ret = __lock_getlocker(lt,
	    parent, locker_ndx, 0, &parent_locker)) != 0)
		goto err;

	/*
	 * If the parent is not there yet, thats ok.  If it
	 * does not have any timouts set, then avoid creating
	 * the child locker at this point.
	 */
	if (parent_locker == NULL ||
	    (LOCK_TIME_ISVALID(&parent_locker->tx_expire) &&
	    !F_ISSET(parent_locker, DB_LOCKER_TIMEOUT))) {
		ret = EINVAL;
		goto done;
	}

	LOCKER_LOCK(lt, region, locker, locker_ndx);
	if ((ret = __lock_getlocker(lt,
	    locker, locker_ndx, 1, &sh_locker)) != 0)
		goto err;

	sh_locker->tx_expire = parent_locker->tx_expire;

	if (F_ISSET(parent_locker, DB_LOCKER_TIMEOUT)) {
		sh_locker->lk_timeout = parent_locker->lk_timeout;
		F_SET(sh_locker, DB_LOCKER_TIMEOUT);
		if (!LOCK_TIME_ISVALID(&parent_locker->tx_expire))
			ret = EINVAL;
	}

done:
err:
	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_expires --
 *	Set the expire time given the time to live.  If timevalp is set then
 *	it contains "now".  This avoids repeated system calls to get the time.
 *
 * PUBLIC: void __lock_expires __P((DB_ENV *, db_timeval_t *, db_timeout_t));
 */
void
__lock_expires(dbenv, timevalp, timeout)
	DB_ENV *dbenv;
	db_timeval_t *timevalp;
	db_timeout_t timeout;
{
	if (!LOCK_TIME_ISVALID(timevalp))
		__os_clock(dbenv, &timevalp->tv_sec, &timevalp->tv_usec);
	if (timeout > 1000000) {
		timevalp->tv_sec += timeout / 1000000;
		timevalp->tv_usec += timeout % 1000000;
	} else
		timevalp->tv_usec += timeout;

	if (timevalp->tv_usec > 1000000) {
		timevalp->tv_sec++;
		timevalp->tv_usec -= 1000000;
	}
}

/*
 * __lock_expired -- determine if a lock has expired.
 *
 * PUBLIC: int __lock_expired __P((DB_ENV *, db_timeval_t *, db_timeval_t *));
 */
int
__lock_expired(dbenv, now, timevalp)
	DB_ENV *dbenv;
	db_timeval_t *now, *timevalp;
{
	if (!LOCK_TIME_ISVALID(timevalp))
		return (0);

	if (!LOCK_TIME_ISVALID(now))
		__os_clock(dbenv, &now->tv_sec, &now->tv_usec);

	return (now->tv_sec > timevalp->tv_sec ||
	    (now->tv_sec == timevalp->tv_sec &&
	    now->tv_usec >= timevalp->tv_usec));
}
