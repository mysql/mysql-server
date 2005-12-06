/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mut_pthread.c,v 12.12 2005/11/01 00:44:27 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/mutex_int.h"

#ifdef HAVE_MUTEX_SOLARIS_LWP
#define	pthread_cond_destroy(x)		0
#define	pthread_cond_signal		_lwp_cond_signal
#define	pthread_cond_wait		_lwp_cond_wait
#define	pthread_mutex_destroy(x)	0
#define	pthread_mutex_lock		_lwp_mutex_lock
#define	pthread_mutex_trylock		_lwp_mutex_trylock
#define	pthread_mutex_unlock		_lwp_mutex_unlock
#endif
#ifdef HAVE_MUTEX_UI_THREADS
#define	pthread_cond_destroy(x)		cond_destroy
#define	pthread_cond_signal		cond_signal
#define	pthread_cond_wait		cond_wait
#define	pthread_mutex_destroy		mutex_destroy
#define	pthread_mutex_lock		mutex_lock
#define	pthread_mutex_trylock		mutex_trylock
#define	pthread_mutex_unlock		mutex_unlock
#endif

#define	PTHREAD_UNLOCK_ATTEMPTS	5

/*
 * IBM's MVS pthread mutex implementation returns -1 and sets errno rather than
 * returning errno itself.  As -1 is not a valid errno value, assume functions
 * returning -1 have set errno.  If they haven't, return a random error value.
 */
#define	RET_SET(f, ret) do {						\
	if (((ret) = (f)) == -1 && ((ret) = errno) == 0)		\
		(ret) = EAGAIN;						\
} while (0)

/*
 * __db_pthread_mutex_init --
 *	Initialize a pthread mutex.
 *
 * PUBLIC: int __db_pthread_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
 */
int
__db_pthread_mutex_init(dbenv, mutex, flags)
	DB_ENV *dbenv;
	db_mutex_t mutex;
	u_int32_t flags;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int ret;

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);
	ret = 0;

#ifdef HAVE_MUTEX_PTHREADS
	{
	pthread_condattr_t condattr, *condattrp = NULL;
	pthread_mutexattr_t mutexattr, *mutexattrp = NULL;

	if (!LF_ISSET(DB_MUTEX_THREAD)) {
		RET_SET((pthread_mutexattr_init(&mutexattr)), ret);
#ifndef HAVE_MUTEX_THREAD_ONLY
		if (ret == 0)
			RET_SET((pthread_mutexattr_setpshared(
			    &mutexattr, PTHREAD_PROCESS_SHARED)), ret);
#endif
		mutexattrp = &mutexattr;
	}

	if (ret == 0)
		RET_SET((pthread_mutex_init(&mutexp->mutex, mutexattrp)), ret);
	if (mutexattrp != NULL)
		(void)pthread_mutexattr_destroy(mutexattrp);
	if (ret == 0 && LF_ISSET(DB_MUTEX_SELF_BLOCK)) {
		if (!LF_ISSET(DB_MUTEX_THREAD)) {
			RET_SET((pthread_condattr_init(&condattr)), ret);
			if (ret == 0) {
				condattrp = &condattr;
#ifndef HAVE_MUTEX_THREAD_ONLY
				RET_SET((pthread_condattr_setpshared(
				    &condattr, PTHREAD_PROCESS_SHARED)), ret);
#endif
			}
		}

		if (ret == 0)
			RET_SET(
			    (pthread_cond_init(&mutexp->cond, condattrp)), ret);

		F_SET(mutexp, DB_MUTEX_SELF_BLOCK);
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
	if (LF_ISSET(DB_MUTEX_THREAD)) {
		static lwp_mutex_t mi = DEFAULTMUTEX;

		mutexp->mutex = mi;
	} else {
		static lwp_mutex_t mi = SHAREDMUTEX;

		mutexp->mutex = mi;
	}
	if (LF_ISSET(DB_MUTEX_SELF_BLOCK)) {
		if (LF_ISSET(DB_MUTEX_THREAD)) {
			static lwp_cond_t ci = DEFAULTCV;

			mutexp->cond = ci;
		} else {
			static lwp_cond_t ci = SHAREDCV;

			mutexp->cond = ci;
		}
		F_SET(mutexp, DB_MUTEX_SELF_BLOCK);
	}
#endif
#ifdef HAVE_MUTEX_UI_THREADS
	{
	int type;

	type = LF_ISSET(DB_MUTEX_THREAD) ? USYNC_THREAD : USYNC_PROCESS;

	ret = mutex_init(&mutexp->mutex, type, NULL);
	if (ret == 0 && LF_ISSET(DB_MUTEX_SELF_BLOCK)) {
		ret = cond_init(&mutexp->cond, type, NULL);

		F_SET(mutexp, DB_MUTEX_SELF_BLOCK);
	}}
#endif

	if (ret != 0) {
		__db_err(dbenv,
		    "unable to initialize mutex: %s", strerror(ret));
	}
	return (ret);
}

/*
 * __db_pthread_mutex_lock
 *	Lock on a mutex, blocking if necessary.
 *
 * PUBLIC: int __db_pthread_mutex_lock __P((DB_ENV *, db_mutex_t));
 */
int
__db_pthread_mutex_lock(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int i, ret;

	if (!MUTEX_ON(dbenv) || F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

#ifdef HAVE_STATISTICS
	/*
	 * We want to know which mutexes are contentious, but don't want to
	 * do an interlocked test here -- that's slower when the underlying
	 * system has adaptive mutexes and can perform optimizations like
	 * spinning only if the thread holding the mutex is actually running
	 * on a CPU.  Make a guess, using a normal load instruction.
	 */
	if (F_ISSET(mutexp, DB_MUTEX_LOCKED))
		++mutexp->mutex_set_wait;
	else
		++mutexp->mutex_set_nowait;
#endif

	RET_SET((pthread_mutex_lock(&mutexp->mutex)), ret);
	if (ret != 0)
		goto err;

	if (F_ISSET(mutexp, DB_MUTEX_SELF_BLOCK)) {
		while (F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
			RET_SET((pthread_cond_wait(
			    &mutexp->cond, &mutexp->mutex)), ret);
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
				goto err;
			}
		}

		F_SET(mutexp, DB_MUTEX_LOCKED);
		dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);
		CHECK_MTX_THREAD(dbenv, mutexp);

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
			RET_SET((pthread_mutex_unlock(&mutexp->mutex)), ret);
		} while (ret == EFAULT && --i > 0);
		if (ret != 0)
			goto err;
	} else {
#ifdef DIAGNOSTIC
		if (F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
			char buf[DB_THREADID_STRLEN];
			(void)dbenv->thread_id_string(dbenv,
			    mutexp->pid, mutexp->tid, buf);
			__db_err(dbenv,
		    "pthread lock failed: lock currently in use: pid/tid: %s",
			    buf);
			ret = EINVAL;
			goto err;
		}
#endif
		F_SET(mutexp, DB_MUTEX_LOCKED);
		dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);
		CHECK_MTX_THREAD(dbenv, mutexp);
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

err:	__db_err(dbenv, "pthread lock failed: %s", db_strerror(ret));
	return (__db_panic(dbenv, ret));
}

/*
 * __db_pthread_mutex_unlock --
 *	Release a mutex.
 *
 * PUBLIC: int __db_pthread_mutex_unlock __P((DB_ENV *, db_mutex_t));
 */
int
__db_pthread_mutex_unlock(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int i, ret;

	if (!MUTEX_ON(dbenv) || F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

#ifdef DIAGNOSTIC
	if (!F_ISSET(mutexp, DB_MUTEX_LOCKED)) {
		__db_err(dbenv, "pthread unlock failed: lock already unlocked");
		return (__db_panic(dbenv, EACCES));
	}
#endif
	if (F_ISSET(mutexp, DB_MUTEX_SELF_BLOCK)) {
		RET_SET((pthread_mutex_lock(&mutexp->mutex)), ret);
		if (ret != 0)
			goto err;

		F_CLR(mutexp, DB_MUTEX_LOCKED);

		RET_SET((pthread_cond_signal(&mutexp->cond)), ret);
		if (ret != 0)
			goto err;
	} else
		F_CLR(mutexp, DB_MUTEX_LOCKED);

	/* See comment above;  workaround for [#2471]. */
	i = PTHREAD_UNLOCK_ATTEMPTS;
	do {
		RET_SET((pthread_mutex_unlock(&mutexp->mutex)), ret);
	} while (ret == EFAULT && --i > 0);

err:	if (ret != 0) {
		__db_err(dbenv, "pthread unlock failed: %s", db_strerror(ret));
		return (__db_panic(dbenv, ret));
	}
	return (ret);
}

/*
 * __db_pthread_mutex_destroy --
 *	Destroy a mutex.
 *
 * PUBLIC: int __db_pthread_mutex_destroy __P((DB_ENV *, db_mutex_t));
 */
int
__db_pthread_mutex_destroy(dbenv, mutex)
	DB_ENV *dbenv;
	db_mutex_t mutex;
{
	DB_MUTEX *mutexp;
	DB_MUTEXMGR *mtxmgr;
	DB_MUTEXREGION *mtxregion;
	int ret, t_ret;

	if (!MUTEX_ON(dbenv))
		return (0);

	mtxmgr = dbenv->mutex_handle;
	mtxregion = mtxmgr->reginfo.primary;
	mutexp = MUTEXP_SET(mutex);

	ret = 0;
	if (F_ISSET(mutexp, DB_MUTEX_SELF_BLOCK)) {
		RET_SET((pthread_cond_destroy(&mutexp->cond)), ret);
		if (ret != 0)
			__db_err(NULL,
			    "unable to destroy cond: %s", strerror(ret));
	}
	RET_SET((pthread_mutex_destroy(&mutexp->mutex)), t_ret);
	if (t_ret != 0) {
		__db_err(NULL, "unable to destroy mutex: %s", strerror(t_ret));
		if (ret == 0)
			ret = t_ret;
	}
	return (ret);
}
