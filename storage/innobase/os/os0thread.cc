/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file os/os0thread.cc
The interface to the operating system thread control primitives

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#include "os0thread.h"
#ifdef UNIV_NONINL
#include "os0thread.ic"
#endif

#ifdef __WIN__
#include <windows.h>
#endif

#ifndef UNIV_HOTBACKUP
#include "srv0srv.h"
#include "os0event.h"

#ifdef UNIV_PFS_MUTEX
/* Key to register server_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	thread_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/** Mutex that tracks the thread count. */
static SysMutex	thread_mutex;

/** Number of threads active. */
UNIV_INTERN	ulint	os_thread_count;

/***************************************************************//**
Compares two thread ids for equality.
@return	TRUE if equal */
UNIV_INTERN
ibool
os_thread_eq(
/*=========*/
	os_thread_id_t	a,	/*!< in: OS thread or thread id */
	os_thread_id_t	b)	/*!< in: OS thread or thread id */
{
#ifdef __WIN__
	if (a == b) {
		return(TRUE);
	}

	return(FALSE);
#else
	if (pthread_equal(a, b)) {
		return(TRUE);
	}

	return(FALSE);
#endif
}

/****************************************************************//**
Converts an OS thread id to a ulint. It is NOT guaranteed that the ulint is
unique for the thread though!
@return	thread identifier as a number */
UNIV_INTERN
ulint
os_thread_pf(
/*=========*/
	os_thread_id_t	a)	/*!< in: OS thread identifier */
{
#ifdef UNIV_HPUX10
	/* In HP-UX-10.20 a pthread_t is a struct of 3 fields: field1, field2,
	field3. We do not know if field1 determines the thread uniquely. */

	return((ulint)(a.field1));
#else
	return((ulint) a);
#endif
}

/*****************************************************************//**
Returns the thread identifier of current thread. Currently the thread
identifier in Unix is the thread handle itself. Note that in HP-UX
pthread_t is a struct of 3 fields.
@return	current thread identifier */
UNIV_INTERN
os_thread_id_t
os_thread_get_curr_id(void)
/*=======================*/
{
#ifdef __WIN__
	return(GetCurrentThreadId());
#else
	return(pthread_self());
#endif
}

/****************************************************************//**
Creates a new thread of execution. The execution starts from
the function given.
NOTE: We count the number of threads in os_thread_exit(). A created
thread should always use that to exit so thatthe thread count will be
decremented.
We do not return an error code because if there is one, we crash here. */
UNIV_INTERN
void
os_thread_create_func(
/*==================*/
	os_thread_func_t	func,		/*!< in: pointer to function
						from which to start */
	void*			arg,		/*!< in: argument to start
						function */
	os_thread_id_t*		thread_id)	/*!< out: id of the created
						thread, or NULL */
{
#ifdef __WIN__
	HANDLE		handle;
	DWORD		win_thread_id;

	mutex_enter(&thread_mutex);
	os_thread_count++;
	mutex_exit(&thread_mutex);

	handle = CreateThread(NULL,	/* no security attributes */
			      0,	/* default size stack */
			      func,
			      arg,
			      0,	/* thread runs immediately */
			      &win_thread_id);

	if (handle) {
		if (thread_id) {
			*thread_id = win_thread_id;
		}

		/* Innodb does not use the handle outside this thread.
		It only uses the thread_id.  So close the handle now.
		The thread object is held open by the thread until it
		exits. */
		Sleep(0);
		CloseHandle(handle);

	} else {
		/* If we cannot start a new thread, life has no meaning. */
		fprintf(stderr,
			"InnoDB: Error: CreateThread returned %d\n",
			GetLastError());
		ut_ad(0);
		exit(1);
	}

#else /* __WIN__ else */

	int		ret;
	os_thread_id_t	pthread_id;
	pthread_attr_t	attr;

#ifndef UNIV_HPUX10
	pthread_attr_init(&attr);
#endif

#ifdef UNIV_AIX
	/* We must make sure a thread stack is at least 32 kB, otherwise
	InnoDB might crash; we do not know if the default stack size on
	AIX is always big enough. An empirical test on AIX-4.3 suggested
	the size was 96 kB, though. */

	ret = pthread_attr_setstacksize(&attr,
					(size_t)(PTHREAD_STACK_MIN
						 + 32 * 1024));
	if (ret) {
		fprintf(stderr,
			"InnoDB: Error: pthread_attr_setstacksize"
			" returned %d\n", ret);
		ut_ad(0);
		exit(1);
	}
#endif
	mutex_enter(&thread_mutex);
	os_thread_count++;
	mutex_exit(&thread_mutex);

#ifdef UNIV_HPUX10
	ret = pthread_create(&pthread_id, pthread_attr_default, func, arg);
#else
	ret = pthread_create(&pthread_id, &attr, func, arg);
#endif
	if (ret) {
		fprintf(stderr,
			"InnoDB: Error: pthread_create returned %d\n", ret);
		ut_ad(0);
		exit(1);
	}

#ifndef UNIV_HPUX10
	pthread_attr_destroy(&attr);
#endif
#endif /* not __WIN__ */
}

/*****************************************************************//**
Exits the current thread. */
UNIV_INTERN
void
os_thread_exit(
/*===========*/
	void*	exit_value)	/*!< in: exit value; in Windows this void*
				is cast as a DWORD */
{
#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "Thread exits, id %lu\n",
		os_thread_pf(os_thread_get_curr_id()));
#endif

#ifdef UNIV_PFS_THREAD
	pfs_delete_thread();
#endif

	mutex_enter(&thread_mutex);
	os_thread_count--;
	mutex_exit(&thread_mutex);

#ifdef __WIN__
	ExitThread((DWORD) exit_value);
#else
	pthread_detach(pthread_self());
	pthread_exit(exit_value);
#endif
}

/*****************************************************************//**
Advises the os to give up remainder of the thread's time slice. */
UNIV_INTERN
void
os_thread_yield(void)
/*=================*/
{
#if defined(__WIN__)
	SwitchToThread();
#elif (defined(HAVE_SCHED_YIELD) && defined(HAVE_SCHED_H))
	sched_yield();
#elif defined(HAVE_PTHREAD_YIELD_ZERO_ARG)
	pthread_yield();
#elif defined(HAVE_PTHREAD_YIELD_ONE_ARG)
	pthread_yield(0);
#else
	os_thread_sleep(0);
#endif
}
#endif /* !UNIV_HOTBACKUP */

/*****************************************************************//**
The thread sleeps at least the time given in microseconds. */
UNIV_INTERN
void
os_thread_sleep(
/*============*/
	ulint	tm)	/*!< in: time in microseconds */
{
#ifdef __WIN__
	Sleep((DWORD) tm / 1000);
#else
	struct timeval	t;

	t.tv_sec = tm / 1000000;
	t.tv_usec = tm % 1000000;

	select(0, NULL, NULL, NULL, &t);
#endif
}

/*****************************************************************//**
Check if there are threads active.
@return true if the thread count > 0. */
UNIV_INTERN
bool
os_thread_active()
/*==============*/
{
	mutex_enter(&thread_mutex);

	bool active = (os_thread_count > 0);

	/* All the threads have exited or are just exiting;
	NOTE that the threads may not have completed their
	exit yet. Should we use pthread_join() to make sure
	they have exited? If we did, we would have to
	remove the pthread_detach() from
	os_thread_exit().  Now we just sleep 0.1
	seconds and hope that is enough! */

	mutex_exit(&thread_mutex);

	return(active);
}

/**
Initializes OS thread management data structures. */
UNIV_INTERN
void
os_thread_init()
/*============*/
{
	mutex_create("thread_mutex", &thread_mutex);
}

/**
Frees OS thread management data structures. */
UNIV_INTERN
void
os_thread_free()
/*============*/
{
	if (os_thread_count != 0) {
		ib_logf(IB_LOG_LEVEL_WARN,
			"Some (%lu) threads are still active", os_thread_count);
	}

	mutex_destroy(&thread_mutex);
}

