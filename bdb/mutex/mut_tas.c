/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mut_tas.c,v 11.18 2000/11/30 00:58:41 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

/*
 * This is where we load in the actual test-and-set mutex code.
 */
#define	LOAD_ACTUAL_MUTEX_CODE
#include "db_int.h"

#ifdef DIAGNOSTIC
#undef	MSG1
#define	MSG1		"mutex_lock: ERROR: lock currently in use: pid: %lu.\n"
#undef	MSG2
#define	MSG2		"mutex_unlock: ERROR: lock already unlocked\n"
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif
#endif

/*
 * __db_tas_mutex_init --
 *	Initialize a MUTEX.
 *
 * PUBLIC: int __db_tas_mutex_init __P((DB_ENV *, MUTEX *, u_int32_t));
 */
int
__db_tas_mutex_init(dbenv, mutexp, flags)
	DB_ENV *dbenv;
	MUTEX *mutexp;
	u_int32_t flags;
{
	/* Check alignment. */
	DB_ASSERT(((db_alignp_t)mutexp & (MUTEX_ALIGN - 1)) == 0);

	memset(mutexp, 0, sizeof(*mutexp));

	/*
	 * If this is a thread lock or the process has told us that there are
	 * no other processes in the environment, use thread-only locks, they
	 * are faster in some cases.
	 *
	 * This is where we decide to ignore locks we don't need to set -- if
	 * the application isn't threaded, there aren't any threads to block.
	 */
	if (LF_ISSET(MUTEX_THREAD) || F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		if (!F_ISSET(dbenv, DB_ENV_THREAD)) {
			F_SET(mutexp, MUTEX_IGNORE);
			return (0);
		}
		F_SET(mutexp, MUTEX_THREAD);
	}

	/* Initialize the lock. */
	if (MUTEX_INIT(&mutexp->tas))
		return (__os_get_errno());

	mutexp->spins = __os_spin();
#ifdef MUTEX_SYSTEM_RESOURCES
	mutexp->reg_off = INVALID_ROFF;
#endif
	F_SET(mutexp, MUTEX_INITED);

	return (0);
}

/*
 * __db_tas_mutex_lock
 *	Lock on a mutex, logically blocking if necessary.
 *
 * PUBLIC: int __db_tas_mutex_lock __P((DB_ENV *, MUTEX *));
 */
int
__db_tas_mutex_lock(dbenv, mutexp)
	DB_ENV *dbenv;
	MUTEX *mutexp;
{
	u_long ms;
	int nspins;

	if (!dbenv->db_mutexlocks || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	ms = 1;

loop:	/* Attempt to acquire the resource for N spins. */
	for (nspins = mutexp->spins; nspins > 0; --nspins) {
#ifdef HAVE_MUTEX_HPPA_MSEM_INIT
relock:
#endif
		if (!MUTEX_SET(&mutexp->tas))
			continue;
#ifdef HAVE_MUTEX_HPPA_MSEM_INIT
		/*
		 * HP semaphores are unlocked automatically when a holding
		 * process exits.  If the mutex appears to be locked
		 * (mutexp->locked != 0) but we got here, assume this has
		 * happened.  Stick our own pid into mutexp->locked and
		 * lock again.  (The default state of the mutexes used to
		 * block in __lock_get_internal is locked, so exiting with
		 * a locked mutex is reasonable behavior for a process that
		 * happened to initialize or use one of them.)
		 */
		if (mutexp->locked != 0) {
			mutexp->locked = (u_int32_t)getpid();
			goto relock;
		}
		/*
		 * If we make it here, locked == 0, the diagnostic won't fire,
		 * and we were really unlocked by someone calling the
		 * DB mutex unlock function.
		 */
#endif
#ifdef DIAGNOSTIC
		if (mutexp->locked != 0) {
			char msgbuf[128];
			(void)snprintf(msgbuf,
			    sizeof(msgbuf), MSG1, (u_long)mutexp->locked);
			(void)write(STDERR_FILENO, msgbuf, strlen(msgbuf));
		}
#endif
#if defined(DIAGNOSTIC) || defined(HAVE_MUTEX_HPPA_MSEM_INIT)
		mutexp->locked = (u_int32_t)getpid();
#endif
		if (ms == 1)
			++mutexp->mutex_set_nowait;
		else
			++mutexp->mutex_set_wait;
		return (0);
	}

	/* Yield the processor; wait 1ms initially, up to 1 second. */
	__os_yield(NULL, ms * USEC_PER_MS);
	if ((ms <<= 1) > MS_PER_SEC)
		ms = MS_PER_SEC;

	goto loop;
}

/*
 * __db_tas_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_tas_mutex_unlock __P((DB_ENV *, MUTEX *));
 */
int
__db_tas_mutex_unlock(dbenv, mutexp)
	DB_ENV *dbenv;
	MUTEX *mutexp;
{
	if (!dbenv->db_mutexlocks || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

#ifdef DIAGNOSTIC
	if (!mutexp->locked)
		(void)write(STDERR_FILENO, MSG2, sizeof(MSG2) - 1);
#endif
#if defined(DIAGNOSTIC) || defined(HAVE_MUTEX_HPPA_MSEM_INIT)
	mutexp->locked = 0;
#endif

	MUTEX_UNSET(&mutexp->tas);

	return (0);
}

/*
 * __db_tas_mutex_destroy --
 *	Destroy a MUTEX.
 *
 * PUBLIC: int __db_tas_mutex_destroy __P((MUTEX *));
 */
int
__db_tas_mutex_destroy(mutexp)
	MUTEX *mutexp;
{
	if (F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	MUTEX_DESTROY(&mutexp->tas);

	return (0);
}
