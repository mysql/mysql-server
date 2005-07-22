/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_pthread.c,v 11.62 2004/09/22 16:27:05 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

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

#ifdef HAVE_MUTEX_SOLARIS_LWP
#define	pthread_cond_destroy(x)		0
#define	pthread_cond_signal		_lwp_cond_signal
#define	pthread_cond_wait		_lwp_cond_wait
#define	pthread_mutex_destroy(x)	0
#define	pthread_mutex_lock		_lwp_mutex_lock
#define	pthread_mutex_trylock		_lwp_mutex_trylock
#define	pthread_mutex_unlock		_lwp_mutex_unlock
/*
 * !!!
 * _lwp_self returns the LWP process ID which isn't a unique per-thread
 * identifier.  Use pthread_self instead, it appears to work even if we
 * are not a pthreads application.
 */
#endif
#ifdef HAVE_MUTEX_UI_THREADS
#define	pthread_cond_destroy(x)		cond_destroy
#define	pthread_cond_signal		cond_signal
#define	pthread_cond_wait		cond_wait
#define	pthread_mutex_destroy		mutex_destroy
#define	pthread_mutex_lock		mutex_lock
#define	pthread_mutex_trylock		mutex_trylock
#define	pthread_mutex_unlock		mutex_unlock
#define	pthread_self			thr_self
#endif

#define	PTHREAD_UNLOCK_ATTEMPTS	5

/*
 * __db_pthread_mutex_init --
 *	Initialize a DB_MUTEX.
 *
 * PUBLIC: int __db_pthread_mutex_init __P((DB_ENV *, DB_MUTEX *, u_int32_t));
 */
int
__db_pthread_mutex_init(dbenv, mutexp, flags)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	u_int32_t flags;
{
	u_int32_t save;
	int ret;

	ret = 0;

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
	}

#ifdef HAVE_MUTEX_PTHREADS
	{
	pthread_condattr_t condattr, *condattrp = NULL;
	pthread_mutexattr_t mutexattr, *mutexattrp = NULL;

	if (!LF_ISSET(MUTEX_THREAD)) {
		ret = pthread_mutexattr_init(&mutexattr);
#ifndef HAVE_MUTEX_THREAD_ONLY
		if (ret == 0)
			ret = pthread_mutexattr_setpshared(
			    &mutexattr, PTHREAD_PROCESS_SHARED);
#endif
		mutexattrp = &mutexattr;
	}

	if (ret == 0)
		ret = pthread_mutex_init(&mutexp->mutex, mutexattrp);
	if (mutexattrp != NULL)
		pthread_mutexattr_destroy(mutexattrp);
	if (ret == 0 && LF_ISSET(MUTEX_SELF_BLOCK)) {
		if (!LF_ISSET(MUTEX_THREAD)) {
			ret = pthread_condattr_init(&condattr);
#ifndef HAVE_MUTEX_THREAD_ONLY
			if (ret == 0) {
				condattrp = &condattr;
				ret = pthread_condattr_setpshared(
				    &condattr, PTHREAD_PROCESS_SHARED);
			}
#endif
		}

		if (ret == 0)
			ret = pthread_cond_init(&mutexp->cond, condattrp);

		F_SET(mutexp, MUTEX_SELF_BLOCK);
		if (condattrp != NULL)
			(void)pthread_condattr_destroy(condattrp);
	}

	}
#endif
#ifdef HAVE_MUTEX_SOLARIS_LWP
	/*
	 * XXX
	 * Gcc complains about missing braces in the static initializations of
	 * lwp_cond_t and lwp_mutex_t structures because the structures contain
	 * sub-structures/unions and the Solaris include file that defines the
	 * initialization values doesn't have surrounding braces.  There's not
	 * much we can do.
	 */
	if (LF_ISSET(MUTEX_THREAD)) {
		static lwp_mutex_t mi = DEFAULTMUTEX;

		mutexp->mutex = mi;
	} else {
		static lwp_mutex_t mi = SHAREDMUTEX;

		mutexp->mutex = mi;
	}
	if (LF_ISSET(MUTEX_SELF_BLOCK)) {
		if (LF_ISSET(MUTEX_THREAD)) {
			static lwp_cond_t ci = DEFAULTCV;

			mutexp->cond = ci;
		} else {
			static lwp_cond_t ci = SHAREDCV;

			mutexp->cond = ci;
		}
		F_SET(mutexp, MUTEX_SELF_BLOCK);
	}
#endif
#ifdef HAVE_MUTEX_UI_THREADS
	{
	int type;

	type = LF_ISSET(MUTEX_THREAD) ? USYNC_THREAD : USYNC_PROCESS;

	ret = mutex_init(&mutexp->mutex, type, NULL);
	if (ret == 0 && LF_ISSET(MUTEX_SELF_BLOCK)) {
		ret = cond_init(&mutexp->cond, type, NULL);

		F_SET(mutexp, MUTEX_SELF_BLOCK);
	}}
#endif

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	mutexp->reg_off = INVALID_ROFF;
#endif
	if (ret == 0)
		F_SET(mutexp, MUTEX_INITED);
	else
		__db_err(dbenv,
		    "unable to initialize mutex: %s", strerror(ret));

	return (ret);
}

/*
 * __db_pthread_mutex_lock
 *	Lock on a mutex, logically blocking if necessary.
 *
 * PUBLIC: int __db_pthread_mutex_lock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_pthread_mutex_lock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	u_int32_t nspins;
	int i, ret, waited;

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	/* Attempt to acquire the resource for N spins. */
	for (nspins = dbenv->tas_spins; nspins > 0; --nspins)
		if (pthread_mutex_trylock(&mutexp->mutex) == 0)
			break;

	if (nspins == 0 && (ret = pthread_mutex_lock(&mutexp->mutex)) != 0)
		goto err;

	if (F_ISSET(mutexp, MUTEX_SELF_BLOCK)) {
		for (waited = 0; mutexp->locked != 0; waited = 1) {
			ret = pthread_cond_wait(&mutexp->cond, &mutexp->mutex);
			/*
			 * !!!
			 * Solaris bug workaround:
			 * pthread_cond_wait() sometimes returns ETIME -- out
			 * of sheer paranoia, check both ETIME and ETIMEDOUT.
			 * We believe this happens when the application uses
			 * SIGALRM for some purpose, e.g., the C library sleep
			 * call, and Solaris delivers the signal to the wrong
			 * LWP.
			 */
			if (ret != 0 && ret != EINTR &&
#ifdef ETIME
			    ret != ETIME &&
#endif
			    ret != ETIMEDOUT) {
				(void)pthread_mutex_unlock(&mutexp->mutex);
				return (ret);
			}
		}

		if (waited)
			++mutexp->mutex_set_wait;
		else
			++mutexp->mutex_set_nowait;

#ifdef DIAGNOSTIC
		mutexp->locked = (u_int32_t)pthread_self();
#else
		mutexp->locked = 1;
#endif
		/*
		 * According to HP-UX engineers contacted by Netscape,
		 * pthread_mutex_unlock() will occasionally return EFAULT
		 * for no good reason on mutexes in shared memory regions,
		 * and the correct caller behavior is to try again.  Do
		 * so, up to PTHREAD_UNLOCK_ATTEMPTS consecutive times.
		 * Note that we don't bother to restrict this to HP-UX;
		 * it should be harmless elsewhere. [#2471]
		 */
		i = PTHREAD_UNLOCK_ATTEMPTS;
		do {
			ret = pthread_mutex_unlock(&mutexp->mutex);
		} while (ret == EFAULT && --i > 0);
		if (ret != 0)
			goto err;
	} else {
		if (nspins == dbenv->tas_spins)
			++mutexp->mutex_set_nowait;
		else if (nspins > 0) {
			++mutexp->mutex_set_spin;
			mutexp->mutex_set_spins += dbenv->tas_spins - nspins;
		} else
			++mutexp->mutex_set_wait;
#ifdef DIAGNOSTIC
		if (mutexp->locked) {
			char msgbuf[128];
			(void)snprintf(msgbuf,
			    sizeof(msgbuf), MSG1, (u_long)mutexp->locked);
			(void)write(STDERR_FILENO, msgbuf, strlen(msgbuf));
		}
		mutexp->locked = (u_int32_t)pthread_self();
#else
		mutexp->locked = 1;
#endif
	}
	return (0);

err:	__db_err(dbenv, "unable to lock mutex: %s", strerror(ret));
	return (ret);
}

/*
 * __db_pthread_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_pthread_mutex_unlock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_pthread_mutex_unlock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	int i, ret;

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

#ifdef DIAGNOSTIC
	if (!mutexp->locked)
		(void)write(STDERR_FILENO, MSG2, sizeof(MSG2) - 1);
#endif

	if (F_ISSET(mutexp, MUTEX_SELF_BLOCK)) {
		if ((ret = pthread_mutex_lock(&mutexp->mutex)) != 0)
			goto err;

		mutexp->locked = 0;

		if ((ret = pthread_cond_signal(&mutexp->cond)) != 0)
			return (ret);

	} else
		mutexp->locked = 0;

	/* See comment above;  workaround for [#2471]. */
	i = PTHREAD_UNLOCK_ATTEMPTS;
	do {
		ret = pthread_mutex_unlock(&mutexp->mutex);
	} while (ret == EFAULT && --i > 0);
	return (ret);

err:	__db_err(dbenv, "unable to unlock mutex: %s", strerror(ret));
	return (ret);
}

/*
 * __db_pthread_mutex_destroy --
 *	Destroy a DB_MUTEX.
 *
 * PUBLIC: int __db_pthread_mutex_destroy __P((DB_MUTEX *));
 */
int
__db_pthread_mutex_destroy(mutexp)
	DB_MUTEX *mutexp;
{
	int ret, t_ret;

	if (F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	ret = 0;
	if (F_ISSET(mutexp, MUTEX_SELF_BLOCK) &&
	    (ret = pthread_cond_destroy(&mutexp->cond)) != 0)
		__db_err(NULL, "unable to destroy cond: %s", strerror(ret));
	if ((t_ret = pthread_mutex_destroy(&mutexp->mutex)) != 0) {
		__db_err(NULL, "unable to destroy mutex: %s", strerror(t_ret));
		if (ret == 0)
			ret = t_ret;
	}
	return (ret);
}
