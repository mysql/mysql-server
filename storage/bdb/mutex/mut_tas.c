/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_tas.c,v 12.14 2005/11/01 14:42:17 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"

/*
 * This is where we load in the actual test-and-set mutex code.
 */
#define	LOAD_ACTUAL_MUTEX_CODE
#include "dbinc/mutex_int.h"

/*
 * __db_tas_mutex_init --
 *	Initialize a test-and-set mutex.
 *
 * PUBLIC: int __db_tas_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
 */
int
__db_tas_mutex_init(dbenv, mutex, flags)
	DB_ENV *dbenv;
	db_mutex_t mutex;
	u_int32_t flags;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int ret;

	COMPQUIET(flags, 0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	/* Check alignment. */
	if (((uintptr_t)mutexp & (dbenv->mutex_align - 1)) != 0) {
		__db_err(dbenv, "TAS: mutex not appropriately aligned");
		return (EINVAL);
	}

	if (MUTEX_INIT(&mutexp->tas)) {
		ret = __os_get_errno();
		__db_err(dbenv,
		    "TAS: mutex initialize: %s", db_strerror(ret));
		return (ret);
	}
	return (0);
}

/*
 * __db_tas_mutex_lock
 *	Lock on a mutex, blocking if necessary.
 *
 * PUBLIC: int __db_tas_mutex_lock __P((DB_ENV *, db_mutex_t));
 */
int
__db_tas_mutex_lock(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	u_int32_t nspins;
	u_long ms, max_ms;

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

	/*
	 * Wait 1ms initially, up to 10ms for mutexes backing logical database
	 * locks, and up to 25 ms for mutual exclusion data structure mutexes.
	 * SR: #7675
	 */
	ms = 1;
	max_ms = F_ISSET(mutexp, DB_MUTEX_LOGICAL_LOCK) ? 10 : 25;

loop:	/* Attempt to acquire the resource for N spins. */
	for (nspins =
	    mtxregion->stat.st_mutex_tas_spins; nspins > 0; --nspins) {
#ifdef HAVE_MUTEX_HPPA_MSEM_INIT
relock:
#endif
#ifdef HAVE_MUTEX_S390_CC_ASSEMBLY
		tsl_t zero = 0;
#endif
		/*
		 * Avoid interlocked instructions until they're likely to
		 * succeed.
		 */
		if (F_ISSET(mutexp, DB_MUTEX_LOCKED) ||
		    !MUTEX_SET(&mutexp->tas)) {
			/*
			 * Some systems (notably those with newer Intel CPUs)
			 * need a small pause here. [#6975]
			 */
#ifdef MUTEX_PAUSE
			MUTEX_PAUSE
#endif
			continue;
		}

#ifdef HAVE_MUTEX_HPPA_MSEM_INIT
		/*
		 * HP semaphores are unlocked automatically when a holding
		 * process exits.  If the mutex appears to be locked
		 * (F_ISSET(DB_MUTEX_LOCKED)) but we got here, assume this
		 * has happened.  Set the pid and tid into the mutex and
		 * lock again.  (The default state of the mutexes used to
		 * block in __lock_get_internal is locked, so exiting with
		 * a locked mutex is reasonable behavior for a process that
		 * happened to initialize or use one of them.)
		 */
		if (F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
			F_SET(mutexp, DB_MUTEX_LOCKED);
			dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);
			CHECK_MTX_THREAD(dbenv, mutexp);
			goto relock;
		}
		/*
		 * If we make it here, the mutex isn't locked, the diagnostic
		 * won't fire, and we were really unlocked by someone calling
		 * the DB mutex unlock function.
		 */
#endif
#ifdef DIAGNOSTIC
		if (F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
			char buf[DB_THREADID_STRLEN];
			__db_err(dbenv,
			      "TAS lock failed: lock currently in use: ID: %s",
			      dbenv->thread_id_string(dbenv,
			      mutexp->pid, mutexp->tid, buf));
			return (__db_panic(dbenv, EACCES));
		}
#endif
		F_SET(mutexp, DB_MUTEX_LOCKED);
		dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);
		CHECK_MTX_THREAD(dbenv, mutexp);

#ifdef DIAGNOSTIC
		/*
		 * We want to switch threads as often as possible.  Yield
		 * every time we get a mutex to ensure contention.
		 */
		if (F_ISSET(dbenv, DB_ENV_YIELDCPU))
			__os_yield(NULL, 1);
#endif
		return (0);
	}

	/*
	 * Yield the processor.
	 */
	__os_yield(NULL, ms * USEC_PER_MS);
	if ((ms <<= 1) > max_ms)
		ms = max_ms;

	/*
	 * We're spinning.  The environment might be hung, and somebody else
	 * has already recovered it.  The first thing recovery does is panic
	 * the environment.  Check to see if we're never going to get this
	 * mutex.
	 */
	PANIC_CHECK(dbenv);

	goto loop;
}

/*
 * __db_tas_mutex_unlock --
 *	Release a mutex.
 *
 * PUBLIC: int __db_tas_mutex_unlock __P((DB_ENV *, db_mutex_t));
 */
int
__db_tas_mutex_unlock(dbenv, mutex)
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
		__db_err(dbenv, "TAS unlock failed: lock already unlocked");
		return (__db_panic(dbenv, EACCES));
	}
#endif
	F_CLR(mutexp, DB_MUTEX_LOCKED);

	MUTEX_UNSET(&mutexp->tas);

	return (0);
}

/*
 * __db_tas_mutex_destroy --
 *	Destroy a mutex.
 *
 * PUBLIC: int __db_tas_mutex_destroy __P((DB_ENV *, db_mutex_t));
 */
int
__db_tas_mutex_destroy(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;

	if (!MUTEX_ON(dbenv))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	MUTEX_DESTROY(&mutexp->tas);

	return (0);
}
