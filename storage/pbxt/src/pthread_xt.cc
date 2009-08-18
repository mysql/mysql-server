/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-03-22	Paul McCullagh
 *
 * H&G2JCtL
 *
 * This file contains windows specific code
 */

#include "xt_config.h"

#ifdef XT_WIN
#include <my_pthread.h>
#else
#include <sys/resource.h>
#endif
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "pthread_xt.h"
#include "thread_xt.h"

#ifdef XT_WIN

void xt_p_init_threading(void)
{
}

int xt_p_set_normal_priority(pthread_t thr)
{
	if (!SetThreadPriority (thr, THREAD_PRIORITY_NORMAL))
		return GetLastError();
	return 0;
}

int xt_p_set_low_priority(pthread_t thr)
{
	if (!SetThreadPriority (thr, THREAD_PRIORITY_LOWEST))
		return GetLastError();
	return 0;
}

int xt_p_set_high_priority(pthread_t thr)
{
	if (!SetThreadPriority (thr, THREAD_PRIORITY_HIGHEST))
		return GetLastError();
	return 0;
}

#define XT_RWLOCK_MAGIC 0x78AC390E

#ifdef XT_THREAD_LOCK_INFO
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr, const char *n)
#else
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr)
#endif
{
	InitializeCriticalSection(&mutex->mt_cs);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_init(&mutex->mt_lock_info, mutex);
	mutex->mt_name = n;
#endif
	return 0;
}

int xt_p_mutex_destroy(xt_mutex_type *mutex)
{
	DeleteCriticalSection(&mutex->mt_cs);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&mutex->mt_lock_info);
#endif
	return 0;
}

int xt_p_mutex_lock(xt_mutex_type *mx)
{
	EnterCriticalSection(&mx->mt_cs);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&mx->mt_lock_info);
#endif
	return 0;
}

int xt_p_mutex_unlock(xt_mutex_type *mx)
{
	LeaveCriticalSection(&mx->mt_cs);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&mx->mt_lock_info);
#endif
	return 0;
}

int xt_p_mutex_trylock(xt_mutex_type *mutex)
{
#if(_WIN32_WINNT >= 0x0400)
	/* NOTE: MySQL bug! was using?!
	 * pthread_mutex_trylock(A) (WaitForSingleObject((A), 0) == WAIT_TIMEOUT)
	 */
	if (TryEnterCriticalSection(&mutex->mt_cs)) {
#ifdef XT_THREAD_LOCK_INFO
		xt_thread_lock_info_add_owner(&mutex->mt_lock_info);
#endif
		return 0;
	}
	return WAIT_TIMEOUT;
#else
	EnterCriticalSection(&mutex->mt_cs);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&mutex->mt_lock_info);
#endif
	return 0;
#endif
}

#ifdef XT_THREAD_LOCK_INFO
int xt_p_rwlock_init(xt_rwlock_type *rwl, const pthread_condattr_t *attr, const char *n)
#else
int xt_p_rwlock_init(xt_rwlock_type *rwl, const pthread_condattr_t *attr)
#endif
{
	int result;

	if (rwl == NULL)
		return ERROR_BAD_ARGUMENTS;

	rwl->rw_sh_count = 0;
	rwl->rw_ex_count = 0;
	rwl->rw_sh_complete_count = 0;

	result = xt_p_mutex_init_with_autoname(&rwl->rw_ex_lock, NULL);
	if (result != 0)
		goto failed;

	result = xt_p_mutex_init_with_autoname(&rwl->rw_sh_lock, NULL);
	if (result != 0)
		goto failed_2;

	result = pthread_cond_init(&rwl->rw_sh_cond, NULL);
	if (result != 0)
		goto failed_3;

	rwl->rw_magic = XT_RWLOCK_MAGIC;
#ifdef XT_THREAD_LOCK_INFO
	rwl->rw_name = n;
	xt_thread_lock_info_init(&rwl->rw_lock_info, rwl);
#endif
	return 0;

	failed_3:
	(void) xt_p_mutex_destroy(&rwl->rw_sh_lock);

	failed_2:
	(void) xt_p_mutex_destroy(&rwl->rw_ex_lock);

	failed:
	return result;
}

int xt_p_rwlock_destroy(xt_rwlock_type *rwl)
{
	int result = 0, result1 = 0, result2 = 0;

	if (rwl == NULL)
		return ERROR_BAD_ARGUMENTS;

	if (rwl->rw_magic != XT_RWLOCK_MAGIC)
		return ERROR_BAD_ARGUMENTS;

	if ((result = xt_p_mutex_lock(&rwl->rw_ex_lock)) != 0)
		return result;

	if ((result = xt_p_mutex_lock(&rwl->rw_sh_lock)) != 0) {
		(void) xt_p_mutex_unlock(&rwl->rw_ex_lock);
		return result;
	}

	/*
	 * Check whether any threads own/wait for the lock (wait for ex.access);
	 * report "BUSY" if so.
	 */
	if (rwl->rw_ex_count > 0 || rwl->rw_sh_count > rwl->rw_sh_complete_count) {
		result = xt_p_mutex_unlock(&rwl->rw_sh_lock);
		result1 = xt_p_mutex_unlock(&rwl->rw_ex_lock);
		result2 = ERROR_BUSY;
	}
	else {
		rwl->rw_magic = 0;

		if ((result = xt_p_mutex_unlock(&rwl->rw_sh_lock)) != 0)
		{
			xt_p_mutex_unlock(&rwl->rw_ex_lock);
			return result;
		}

		if ((result = xt_p_mutex_unlock(&rwl->rw_ex_lock)) != 0)
			return result;

		result = pthread_cond_destroy(&rwl->rw_sh_cond);
		result1 = xt_p_mutex_destroy(&rwl->rw_sh_lock);
		result2 = xt_p_mutex_destroy(&rwl->rw_ex_lock);
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&rwl->rw_lock_info);
#endif

	return (result != 0) ? result : ((result1 != 0) ? result1 : result2);
}


int xt_p_rwlock_rdlock(xt_rwlock_type *rwl)
{
	int result;

	if (rwl == NULL)
		return ERROR_BAD_ARGUMENTS;

	if (rwl->rw_magic != XT_RWLOCK_MAGIC)
		return ERROR_BAD_ARGUMENTS;

	if ((result = xt_p_mutex_lock(&rwl->rw_ex_lock)) != 0)
		return result;

	if (++rwl->rw_sh_count == INT_MAX) {
		if ((result = xt_p_mutex_lock(&rwl->rw_sh_lock)) != 0)
		{
			(void) xt_p_mutex_unlock(&rwl->rw_ex_lock);
			return result;
		}

		rwl->rw_sh_count -= rwl->rw_sh_complete_count;
		rwl->rw_sh_complete_count = 0;

		if ((result = xt_p_mutex_unlock(&rwl->rw_sh_lock)) != 0)
		{
			(void) xt_p_mutex_unlock(&rwl->rw_ex_lock);
			return result;
		}
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&rwl->rw_lock_info);
#endif

	return (xt_p_mutex_unlock (&(rwl->rw_ex_lock)));
}

int xt_p_rwlock_wrlock(xt_rwlock_type *rwl)
{
	int result;

	if (rwl == NULL)
		return ERROR_BAD_ARGUMENTS;

	if (rwl->rw_magic != XT_RWLOCK_MAGIC)
		return ERROR_BAD_ARGUMENTS;

	if ((result = xt_p_mutex_lock (&rwl->rw_ex_lock)) != 0)
		return result;

	if ((result = xt_p_mutex_lock (&rwl->rw_sh_lock)) != 0) {
		(void) xt_p_mutex_unlock (&rwl->rw_ex_lock);
		return result;
	}

	if (rwl->rw_ex_count == 0) {
		if (rwl->rw_sh_complete_count > 0) {
			rwl->rw_sh_count -= rwl->rw_sh_complete_count;
			rwl->rw_sh_complete_count = 0;
		}

		if (rwl->rw_sh_count > 0) {
			rwl->rw_sh_complete_count = -rwl->rw_sh_count;

			do {
				result = pthread_cond_wait (&rwl->rw_sh_cond, &rwl->rw_sh_lock.mt_cs);
			}
			while (result == 0 && rwl->rw_sh_complete_count < 0);

			if (result == 0)
				rwl->rw_sh_count = 0;
		}
	}

	if (result == 0)
		rwl->rw_ex_count++;

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&rwl->rw_lock_info);
#endif

	return result;
}

int xt_p_rwlock_unlock(xt_rwlock_type *rwl)
{
	int result, result1;

	if (rwl == NULL)
		return (ERROR_BAD_ARGUMENTS);

	if (rwl->rw_magic != XT_RWLOCK_MAGIC)
		return ERROR_BAD_ARGUMENTS;

	if (rwl->rw_ex_count == 0) {
		if ((result = xt_p_mutex_lock(&rwl->rw_sh_lock)) != 0)
			return result;

		if (++rwl->rw_sh_complete_count == 0)
			result = pthread_cond_signal(&rwl->rw_sh_cond);

		result1 = xt_p_mutex_unlock(&rwl->rw_sh_lock);
	}
	else {
		rwl->rw_ex_count--;

		result = xt_p_mutex_unlock(&rwl->rw_sh_lock);
		result1 = xt_p_mutex_unlock(&rwl->rw_ex_lock);
	}

#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&rwl->rw_lock_info);
#endif

	return ((result != 0) ? result : result1);
}

int xt_p_cond_wait(xt_cond_type *cond, xt_mutex_type *mutex)
{
	return xt_p_cond_timedwait(cond, mutex, NULL);
}

int xt_p_cond_timedwait(xt_cond_type *cond, xt_mutex_type *mt, struct timespec *abstime)
{
	pthread_mutex_t	*mutex = &mt->mt_cs;
	int				result;
	long			timeout; 
	union ft64		now;

	if (abstime != NULL) {
		GetSystemTimeAsFileTime(&now.ft);

		timeout = (long)((abstime->tv.i64 - now.i64) / 10000);
		if (timeout < 0)
			timeout = 0L;
		if (timeout > abstime->max_timeout_msec)
			timeout = abstime->max_timeout_msec;
	}
	else
		timeout= INFINITE;

	WaitForSingleObject(cond->broadcast_block_event, INFINITE);

	EnterCriticalSection(&cond->lock_waiting);
	cond->waiting++;
	LeaveCriticalSection(&cond->lock_waiting);

	LeaveCriticalSection(mutex);

	result= WaitForMultipleObjects(2, cond->events, FALSE, timeout);

	EnterCriticalSection(&cond->lock_waiting);
	cond->waiting--;
	
	if (cond->waiting == 0) {
		/* The last waiter must reset the broadcast
		 * state (whther there was a broadcast or not)!
		 */
		ResetEvent(cond->events[xt_cond_type::BROADCAST]);
		SetEvent(cond->broadcast_block_event);
	}
	LeaveCriticalSection(&cond->lock_waiting);
	
	EnterCriticalSection(mutex);

	return result == WAIT_TIMEOUT ? ETIMEDOUT : 0;
}

int xt_p_join(pthread_t thread, void **value)
{
	DWORD exitcode;

	while(1) {
		switch (WaitForSingleObject(thread, 10000)) {
			case WAIT_OBJECT_0:
				return 0;
			case WAIT_TIMEOUT:
				/* Don't do this! According to the Win docs:
				 * _endthread automatically closes the thread handle
				 * (whereas _endthreadex does not). Therefore, when using
				 * _beginthread and _endthread, do not explicitly close the
				 * thread handle by calling the Win32 CloseHandle API.
				CloseHandle(thread);
				 */
				/* This is done so that if the thread was not [yet] in the running
				 * state when this function was called we won't deadlock here.
				 */
				if (GetExitCodeThread(thread, &exitcode) && (exitcode == STILL_ACTIVE))
					break;
				return 0;
			case WAIT_FAILED:
				return GetLastError();
		}
	}

	return 0;
}

#else // XT_WIN

#ifdef __darwin__
#define POLICY			SCHED_RR
#else
#define POLICY			pth_policy
#endif

static int pth_policy;
static int pth_normal_priority;
static int pth_min_priority;
static int pth_max_priority;

/* Return zero if the priority was set OK,
 * else errno.
 */
static int pth_set_priority(pthread_t thread, int priority)
{
	struct sched_param	sp;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = priority;
	return pthread_setschedparam(thread, POLICY, &sp);
}

static void pth_get_priority_limits(void)
{
	XTThreadPtr			self = NULL;
	struct sched_param	sp;
	int					err;
	int					start;

	/* Save original priority: */
	err = pthread_getschedparam(pthread_self(), &pth_policy, &sp);
	if (err) {
		xt_throw_errno(XT_CONTEXT, err);
		return;
	}
	pth_normal_priority = sp.sched_priority;

	start = sp.sched_priority;

#ifdef XT_FREEBSD 
	pth_min_priority = sched_get_priority_min(sched_getscheduler(0));
	pth_max_priority = sched_get_priority_max(sched_getscheduler(0));
#else
	/* Search for the minimum priority: */
	pth_min_priority = start;
	for (;;) {
		/* 2007-03-01: Corrected, pth_set_priority returns the error code
		 * (thanks to Hakan for pointing out this bug!)
		 */
		if (pth_set_priority(pthread_self(), pth_min_priority-1) != 0)
			break;
		pth_min_priority--;
	}

	/* Search for the maximum priority: */
	pth_max_priority = start;
	for (;;) {
		if (pth_set_priority(pthread_self(), pth_max_priority+1) != 0)
			break;
		pth_max_priority++;
	}

	/* Restore original priority: */
	pthread_setschedparam(pthread_self(), pth_policy, &sp);
#endif
}

xtPublic void xt_p_init_threading(void)
{
	pth_get_priority_limits();
}

xtPublic int xt_p_set_low_priority(pthread_t thr)
{
	if (pth_min_priority == pth_max_priority) {
		/* Under Linux the priority of normal (non-runtime)
		 * threads are set using the standard methods
		 * for setting process priority.
		 */

		/* We could set who == 0 because it should have the same affect
		 * as using the PID.
		 */

		/* -20 = highest, 20 = lowest */
		if (setpriority(PRIO_PROCESS, getpid(), 20) == -1)
			return errno;
		return 0;
	}
	return pth_set_priority(thr, pth_min_priority);
}

xtPublic int xt_p_set_normal_priority(pthread_t thr)
{
	if (pth_min_priority == pth_max_priority) {
		if (setpriority(PRIO_PROCESS, getpid(), 0) == -1)
			return errno;
		return 0;
	}
	return pth_set_priority(thr, pth_normal_priority);
}

xtPublic int xt_p_set_high_priority(pthread_t thr)
{
	if (pth_min_priority == pth_max_priority) {
		if (setpriority(PRIO_PROCESS, getpid(), -20) == -1)
			return errno;
		return 0;
	}
	return pth_set_priority(thr, pth_max_priority);
}

#ifdef DEBUG_LOCKING

xtPublic int xt_p_mutex_lock(xt_mutex_type *mutex, u_int line, const char *file)
{
	XTThreadPtr self = xt_get_self();
	int			r;

	ASSERT_NS(mutex->mu_init == 12345);
	r = pthread_mutex_lock(&mutex->mu_plock);
	if (r == 0) {
		if (mutex->mu_trace)
			printf("==LOCK mutex %d %s:%d\n", (int) mutex->mu_trace, file, (int) line);
		ASSERT_NS(!mutex->mu_locker);
		mutex->mu_locker = self;
		mutex->mu_line = line;
		mutex->mu_file = file;
	}
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&mutex->mu_lock_info);
#endif
	return r;
}

xtPublic int xt_p_mutex_unlock(xt_mutex_type *mutex)
{
	XTThreadPtr self = xt_get_self();

	ASSERT_NS(mutex->mu_init == 12345);
	ASSERT_NS(mutex->mu_locker == self);
	mutex->mu_locker = NULL;
	if (mutex->mu_trace)
		printf("UNLOCK mutex %d\n", (int) mutex->mu_trace);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&mutex->mu_lock_info);
#endif
	return pthread_mutex_unlock(&mutex->mu_plock);
}

xtPublic int xt_p_mutex_destroy(xt_mutex_type *mutex)
{
	ASSERT_NS(mutex->mu_init == 12345);
	mutex->mu_init = 89898;
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&mutex->mu_lock_info);
#endif
	return pthread_mutex_destroy(&mutex->mu_plock);
}

xtPublic int xt_p_mutex_trylock(xt_mutex_type *mutex)
{
	XTThreadPtr self = xt_get_self();
	int			r;

	ASSERT_NS(mutex->mu_init == 12345);
	r = pthread_mutex_trylock(&mutex->mu_plock);
	if (r == 0) {
		ASSERT_NS(!mutex->mu_locker);
		mutex->mu_locker = self;
#ifdef XT_THREAD_LOCK_INFO
		xt_thread_lock_info_add_owner(&mutex->mu_lock_info);
#endif
	}
	return r;
}

#ifdef XT_THREAD_LOCK_INFO
xtPublic int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr, const char *n)
#else
xtPublic int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr)
#endif
{
	mutex->mu_init = 12345;
	mutex->mu_trace = FALSE;
	mutex->mu_locker = NULL;
#ifdef XT_THREAD_LOCK_INFO
	mutex->mu_name = n;
	xt_thread_lock_info_init(&mutex->mu_lock_info, mutex);
#endif
	return pthread_mutex_init(&mutex->mu_plock, attr);
}

xtPublic int xt_p_cond_wait(xt_cond_type *cond, xt_mutex_type *mutex)
{
	XTThreadPtr self = xt_get_self();
	int			r;

	ASSERT_NS(mutex->mu_init == 12345);
	ASSERT_NS(mutex->mu_locker == self);
	mutex->mu_locker = NULL;
	r = pthread_cond_wait(cond, &mutex->mu_plock);
	ASSERT_NS(!mutex->mu_locker);
	mutex->mu_locker = self;
	return r;
}

xtPublic int xt_p_cond_timedwait(xt_cond_type *cond, xt_mutex_type *mutex, const struct timespec *abstime)
{
	XTThreadPtr self = xt_get_self();
	int			r;

	ASSERT_NS(mutex->mu_init == 12345);
	ASSERT_NS(mutex->mu_locker == self);
	mutex->mu_locker = NULL;
	r = pthread_cond_timedwait(cond, &mutex->mu_plock, abstime);
	ASSERT_NS(!mutex->mu_locker);
	mutex->mu_locker = self;
	return r;
}

xtPublic int xt_p_rwlock_rdlock(xt_rwlock_type *rwlock)
{
	int r;

	ASSERT_NS(rwlock->rw_init == 67890);
	r = pthread_rwlock_rdlock(&rwlock->rw_plock);
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&rwlock->rw_lock_info);
#endif
	return r;
}

xtPublic int xt_p_rwlock_wrlock(xt_rwlock_type *rwlock)
{
	XTThreadPtr self = xt_get_self();
	int			r;

	ASSERT_NS(rwlock->rw_init == 67890);
	r = pthread_rwlock_wrlock(&rwlock->rw_plock);
	if (r == 0) {
		ASSERT_NS(!rwlock->rw_locker);
		rwlock->rw_locker = self;
	}
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_add_owner(&rwlock->rw_lock_info);
#endif
	return r;
}

xtPublic int xt_p_rwlock_unlock(xt_rwlock_type *rwlock)
{
	XTThreadPtr self = xt_get_self();

	ASSERT_NS(rwlock->rw_init == 67890);
	if (rwlock->rw_locker) {
		ASSERT_NS(rwlock->rw_locker == self);
		rwlock->rw_locker = NULL;
	}
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_release_owner(&rwlock->rw_lock_info);
#endif
	return pthread_rwlock_unlock(&rwlock->rw_plock);
}

xtPublic int xt_p_rwlock_destroy(xt_rwlock_type *rwlock)
{
	ASSERT_NS(rwlock->rw_init == 67890);
	rwlock->rw_init = 0;
#ifdef XT_THREAD_LOCK_INFO
	xt_thread_lock_info_free(&rwlock->rw_lock_info);
#endif
	return pthread_rwlock_destroy(&rwlock->rw_plock);
}

#ifdef XT_THREAD_LOCK_INFO
xtPublic int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_rwlockattr_t *attr, const char *n)
#else
xtPublic int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_rwlockattr_t *attr)
#endif
{
	rwlock->rw_init = 67890;
	rwlock->rw_readers = 0;
	rwlock->rw_locker = NULL;
#ifdef XT_THREAD_LOCK_INFO
	rwlock->rw_name = n;
	xt_thread_lock_info_init(&rwlock->rw_lock_info, rwlock);
#endif
	return pthread_rwlock_init(&rwlock->rw_plock, attr);
}

#endif // DEBUG_LOCKING

#endif // XT_WIN

