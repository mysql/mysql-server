/*
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mut_win32.c,v 1.8 2002/09/10 02:37:25 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

/*
 * This is where we load in the actual test-and-set mutex code.
 */
#define	LOAD_ACTUAL_MUTEX_CODE
#include "db_int.h"

/* We don't want to run this code even in "ordinary" diagnostic mode. */
#undef MUTEX_DIAG

#define	GET_HANDLE(mutexp, event) do {					\
	char idbuf[13];							\
									\
	if (F_ISSET(mutexp, MUTEX_THREAD)) {				\
		event = mutexp->event;					\
		return (0);						\
	}								\
									\
	snprintf(idbuf, sizeof idbuf, "db.m%08x", mutexp->id);		\
	event = CreateEvent(NULL, FALSE, FALSE, idbuf);			\
	if (event == NULL)						\
		return (__os_win32_errno());				\
} while (0)

#define	RELEASE_HANDLE(mutexp, event)					\
	if (!F_ISSET(mutexp, MUTEX_THREAD) && event != NULL) {		\
		CloseHandle(event);					\
		event = NULL;						\
	}

/*
 * __db_win32_mutex_init --
 *	Initialize a DB_MUTEX.
 *
 * PUBLIC: int __db_win32_mutex_init __P((DB_ENV *, DB_MUTEX *, u_int32_t));
 */
int
__db_win32_mutex_init(dbenv, mutexp, flags)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	u_int32_t flags;
{
	u_int32_t save;

	/*
	 * The only setting/checking of the MUTEX_MPOOL flags is in the mutex
	 * mutex allocation code (__db_mutex_alloc/free).  Preserve only that
	 * flag.  This is safe because even if this flag was never explicitly
	 * set, but happened to be set in memory, it will never be checked or
	 * acted upon.
	 */
	save = F_ISSET(mutexp, MUTEX_MPOOL);
	memset(mutexp, 0, sizeof(*mutexp));
	F_SET(mutexp, save);

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
		mutexp->event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (mutexp->event == NULL)
			return (__os_win32_errno());
	} else
		mutexp->id = ((getpid() & 0xffff) << 16) ^ P_TO_UINT32(mutexp);

	mutexp->spins = __os_spin(dbenv);
	F_SET(mutexp, MUTEX_INITED);

	return (0);
}

/*
 * __db_win32_mutex_lock
 *	Lock on a mutex, logically blocking if necessary.
 *
 * PUBLIC: int __db_win32_mutex_lock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_win32_mutex_lock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	HANDLE event;
	int ret, ms, nspins;
#ifdef MUTEX_DIAG
	LARGE_INTEGER now;
#endif

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	event = NULL;
	ms = 50;
	ret = 0;

loop:	/* Attempt to acquire the resource for N spins. */
	for (nspins = mutexp->spins; nspins > 0; --nspins) {
		if (!MUTEX_SET(&mutexp->tas))
			continue;

#ifdef DIAGNOSTIC
		if (mutexp->locked)
			__db_err(dbenv,
			    "__db_win32_mutex_lock: mutex double-locked!");

		__os_id(&mutexp->locked);
#endif

		if (event == NULL)
			++mutexp->mutex_set_nowait;
		else {
			++mutexp->mutex_set_wait;
			RELEASE_HANDLE(mutexp, event);
			InterlockedDecrement(&mutexp->nwaiters);
#ifdef MUTEX_DIAG
			if (ret != WAIT_OBJECT_0) {
				QueryPerformanceCounter(&now);
				printf("[%I64d]: Lost signal on mutex %p, "
				    "id %d, ms %d\n",
				    now.QuadPart, mutexp, mutexp->id, ms);
			}
#endif
		}

		return (0);
	}

	/*
	 * Yield the processor; wait 50 ms initially, up to 1 second.  This
	 * loop is needed to work around a race where the signal from the
	 * unlocking thread gets lost.  We start at 50 ms because it's unlikely
	 * to happen often and we want to avoid wasting CPU.
	 */
	if (event == NULL) {
#ifdef MUTEX_DIAG
		QueryPerformanceCounter(&now);
		printf("[%I64d]: Waiting on mutex %p, id %d\n",
		    now.QuadPart, mutexp, mutexp->id);
#endif
		InterlockedIncrement(&mutexp->nwaiters);
		GET_HANDLE(mutexp, event);
	}
	if ((ret = WaitForSingleObject(event, ms)) == WAIT_FAILED)
		return (__os_win32_errno());
	if ((ms <<= 1) > MS_PER_SEC)
		ms = MS_PER_SEC;

	goto loop;
}

/*
 * __db_win32_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_win32_mutex_unlock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_win32_mutex_unlock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	int ret;
	HANDLE event;
#ifdef MUTEX_DIAG
		LARGE_INTEGER now;
#endif

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

#ifdef DIAGNOSTIC
	if (!mutexp->tas || !mutexp->locked)
		__db_err(dbenv,
		    "__db_win32_mutex_unlock: ERROR: lock already unlocked");

	mutexp->locked = 0;
#endif
	MUTEX_UNSET(&mutexp->tas);

	ret = 0;
	
	if (mutexp->nwaiters > 0) {
		GET_HANDLE(mutexp, event);

#ifdef MUTEX_DIAG
		QueryPerformanceCounter(&now);
		printf("[%I64d]: Signalling mutex %p, id %d\n",
		    now.QuadPart, mutexp, mutexp->id);
#endif
		if (!PulseEvent(event))
			ret = __os_win32_errno();

		RELEASE_HANDLE(mutexp, event);
	}

#ifdef DIAGNOSTIC
	if (ret != 0)
		__db_err(dbenv,
		    "__db_win32_mutex_unlock: ERROR: unlock failed");
#endif

	return (ret);
}

/*
 * __db_win32_mutex_destroy --
 *	Destroy a DB_MUTEX.
 *
 * PUBLIC: int __db_win32_mutex_destroy __P((DB_MUTEX *));
 */
int
__db_win32_mutex_destroy(mutexp)
	DB_MUTEX *mutexp;
{
	int ret;

	if (F_ISSET(mutexp, MUTEX_IGNORE) || !F_ISSET(mutexp, MUTEX_THREAD))
		return (0);

	ret = 0;
	if (mutexp->event != NULL) {
		if (!CloseHandle(mutexp->event))
			ret = __os_win32_errno();
		mutexp->event = NULL;
	}

	return (ret);
}
