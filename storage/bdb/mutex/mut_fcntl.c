/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_fcntl.c,v 11.26 2004/01/28 03:36:18 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>				/* SEEK_SET on SunOS. */
#endif

#include "db_int.h"

/*
 * __db_fcntl_mutex_init --
 *	Initialize a DB mutex structure.
 *
 * PUBLIC: int __db_fcntl_mutex_init __P((DB_ENV *, DB_MUTEX *, u_int32_t));
 */
int
__db_fcntl_mutex_init(dbenv, mutexp, offset)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	u_int32_t offset;
{
	u_int32_t save;

	/*
	 * The only setting/checking of the MUTEX_MPOOL flag is in the mutex
	 * mutex allocation code (__db_mutex_alloc/free).  Preserve only that
	 * flag.  This is safe because even if this flag was never explicitly
	 * set, but happened to be set in memory, it will never be checked or
	 * acted upon.
	 */
	save = F_ISSET(mutexp, MUTEX_MPOOL);
	memset(mutexp, 0, sizeof(*mutexp));
	F_SET(mutexp, save);

	/*
	 * This is where we decide to ignore locks we don't need to set -- if
	 * the application is private, we don't need any locks.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		F_SET(mutexp, MUTEX_IGNORE);
		return (0);
	}

	mutexp->off = offset;
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	mutexp->reg_off = INVALID_ROFF;
#endif
	F_SET(mutexp, MUTEX_INITED);

	return (0);
}

/*
 * __db_fcntl_mutex_lock
 *	Lock on a mutex, blocking if necessary.
 *
 * PUBLIC: int __db_fcntl_mutex_lock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_fcntl_mutex_lock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	struct flock k_lock;
	int locked, ms, waited;

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	/* Initialize the lock. */
	k_lock.l_whence = SEEK_SET;
	k_lock.l_start = mutexp->off;
	k_lock.l_len = 1;

	for (locked = waited = 0;;) {
		/*
		 * Wait for the lock to become available; wait 1ms initially,
		 * up to 1 second.
		 */
		for (ms = 1; mutexp->pid != 0;) {
			waited = 1;
			__os_yield(NULL, ms * USEC_PER_MS);
			if ((ms <<= 1) > MS_PER_SEC)
				ms = MS_PER_SEC;
		}

		/* Acquire an exclusive kernel lock. */
		k_lock.l_type = F_WRLCK;
		if (fcntl(dbenv->lockfhp->fd, F_SETLKW, &k_lock))
			return (__os_get_errno());

		/* If the resource is still available, it's ours. */
		if (mutexp->pid == 0) {
			locked = 1;
			__os_id(&mutexp->pid);
		}

		/* Release the kernel lock. */
		k_lock.l_type = F_UNLCK;
		if (fcntl(dbenv->lockfhp->fd, F_SETLK, &k_lock))
			return (__os_get_errno());

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

	if (waited)
		++mutexp->mutex_set_wait;
	else
		++mutexp->mutex_set_nowait;
	return (0);
}

/*
 * __db_fcntl_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_fcntl_mutex_unlock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_fcntl_mutex_unlock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

#ifdef DIAGNOSTIC
#define	MSG		"mutex_unlock: ERROR: released lock that was unlocked\n"
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif
	if (mutexp->pid == 0)
		write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
#endif

	/*
	 * Release the resource.  We don't have to acquire any locks because
	 * processes trying to acquire the lock are checking for a pid set to
	 * 0/non-0, not to any specific value.
	 */
	mutexp->pid = 0;

	return (0);
}

/*
 * __db_fcntl_mutex_destroy --
 *	Destroy a DB_MUTEX.
 *
 * PUBLIC: int __db_fcntl_mutex_destroy __P((DB_MUTEX *));
 */
int
__db_fcntl_mutex_destroy(mutexp)
	DB_MUTEX *mutexp;
{
	COMPQUIET(mutexp, NULL);

	return (0);
}
