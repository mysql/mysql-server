/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_fcntl.c,v 12.13 2005/11/01 14:42:17 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/mutex_int.h"

/*
 * __db_fcntl_mutex_init --
 *	Initialize a fcntl mutex.
 *
 * PUBLIC: int __db_fcntl_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
 */
int
__db_fcntl_mutex_init(dbenv, mutex, flags)
	DB_ENV *dbenv;
	db_mutex_t mutex;
	u_int32_t flags;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(mutex, MUTEX_INVALID);
	COMPQUIET(flags, 0);

	return (0);
}

/*
 * __db_fcntl_mutex_lock
 *	Lock on a mutex, blocking if necessary.
 *
 * PUBLIC: int __db_fcntl_mutex_lock __P((DB_ENV *, db_mutex_t));
 */
int
__db_fcntl_mutex_lock(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	struct flock k_lock;
	int locked, ms, ret;

	if (!MUTEX_ON(dbenv) || F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

#ifdef HAVE_STATISTICS
	if (F_ISSET(mutexp, DB_MUTEX_LOCKED))
		++mutexp->mutex_set_wait;
	else
		++mutexp->mutex_set_nowait;
#endif

	/* Initialize the lock. */
	k_lock.l_whence = SEEK_SET;
	k_lock.l_start = mutex;
	k_lock.l_len = 1;

	for (locked = 0;;) {
		/*
		 * Wait for the lock to become available; wait 1ms initially,
		 * up to 1 second.
		 */
		for (ms = 1; F_ISSET(mutexp, DB_MUTEX_LOCKED);) {
			__os_yield(NULL, ms * USEC_PER_MS);
			if ((ms <<= 1) > MS_PER_SEC)
				ms = MS_PER_SEC;
		}

		/* Acquire an exclusive kernel lock. */
		k_lock.l_type = F_WRLCK;
		if (fcntl(dbenv->lockfhp->fd, F_SETLKW, &k_lock))
			goto err;

		/* If the resource is still available, it's ours. */
		if (!F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
			locked = 1;

			F_SET(mutexp, DB_MUTEX_LOCKED);
			dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);
			CHECK_MTX_THREAD(dbenv, mutexp);
		}

		/* Release the kernel lock. */
		k_lock.l_type = F_UNLCK;
		if (fcntl(dbenv->lockfhp->fd, F_SETLK, &k_lock))
			goto err;

		/*
		 * If we got the resource lock we're done.
		 *
		 * !!!
		 * We can't check to see if the lock is ours, because we may
		 * be trying to block ourselves in the lock manager, and so
		 * the holder of the lock that's preventing us from getting
		 * the lock may be us!  (Seriously.)
		 */
		if (locked)
			break;
	}

#ifdef DIAGNOSTIC
	/*
	 * We want to switch threads as often as possible.  Yield every time
	 * we get a mutex to ensure contention.
	 */
	if (F_ISSET(dbenv, DB_ENV_YIELDCPU))
		__os_yield(NULL, 1);
#endif
	return (0);

err:	ret = __os_get_errno();
	__db_err(dbenv, "fcntl lock failed: %s", db_strerror(ret));
	return (__db_panic(dbenv, ret));
}

/*
 * __db_fcntl_mutex_unlock --
 *	Release a mutex.
 *
 * PUBLIC: int __db_fcntl_mutex_unlock __P((DB_ENV *, db_mutex_t));
 */
int
__db_fcntl_mutex_unlock(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;

	if (!MUTEX_ON(dbenv) || F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

#ifdef DIAGNOSTIC
	if (!F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
		__db_err(dbenv, "fcntl unlock failed: lock already unlocked");
		return (__db_panic(dbenv, EACCES));
	}
#endif

	/*
	 * Release the resource.  We don't have to acquire any locks because
	 * processes trying to acquire the lock are waiting for the flag to
	 * go to 0.  Once that happens the waiters will serialize acquiring 
	 * an exclusive kernel lock before locking the mutex.
	 */
	F_CLR(mutexp, DB_MUTEX_LOCKED);

	return (0);
}

/*
 * __db_fcntl_mutex_destroy --
 *	Destroy a mutex.
 *
 * PUBLIC: int __db_fcntl_mutex_destroy __P((DB_ENV *, db_mutex_t));
 */
int
__db_fcntl_mutex_destroy(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(mutex, MUTEX_INVALID);

	return (0);
}
