/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef UNIV_HOTBACKUP
#include "srv0srv.h"
#include "os0event.h"

#include <map>

#ifdef UNIV_PFS_MUTEX
/* Key to register server_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	thread_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/** Mutex that tracks the thread count. */
static SysMutex	thread_mutex;

/** Number of threads active. */
UNIV_INTERN	ulint	os_thread_count;

#ifdef _WIN32
typedef std::map<DWORD, HANDLE> WinThreadMap;
/** This STL map remembers the initial handle returned by CreateThread
so that it can be closed when the thread exits. */
static WinThreadMap win_thread_map;
#endif /* _WIN32 */

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
#ifdef _WIN32
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
	return((ulint) a);
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
#ifdef _WIN32
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
	os_thread_id_t	new_thread_id;

#ifdef _WIN32
	HANDLE		handle;

	handle = CreateThread(NULL,	/* no security attributes */
			      0,	/* default size stack */
			      func,
			      arg,
			      0,	/* thread runs immediately */
			      &new_thread_id);

	if (!handle) {
		/* If we cannot start a new thread, life has no meaning. */
		ib_logf(IB_LOG_LEVEL_FATAL,
			"CreateThread returned %d", GetLastError());
		ut_ad(0);
		exit(1);
	}

	mutex_enter(&thread_mutex);

	std::pair<WinThreadMap::iterator, bool> ret;

	ret = win_thread_map.insert(
		std::pair<DWORD, HANDLE>(new_thread_id, handle));

	ut_ad((*ret.first).first == new_thread_id);
	ut_ad((*ret.first).second == handle);
	ut_a(ret.second == true);	/* true means thread_id was new */

	os_thread_count++;

	mutex_exit(&thread_mutex);

#else /* _WIN32 else */

	pthread_attr_t	attr;

	pthread_attr_init(&attr);

	mutex_enter(&thread_mutex);
	++os_thread_count;
	mutex_exit(&thread_mutex);

	int	ret = pthread_create(&new_thread_id, &attr, func, arg);

	if (ret != 0) {
		ib_logf(IB_LOG_LEVEL_FATAL,
			"pthread_create returned %d", ret);
	}

	pthread_attr_destroy(&attr);

#endif /* not _WIN32 */

	/* Return the thread_id if the caller requests it. */
	if (thread_id != NULL) {
		*thread_id = new_thread_id;
	}
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

#ifdef _WIN32
	DWORD win_thread_id = GetCurrentThreadId();
	HANDLE handle = win_thread_map[win_thread_id];
	CloseHandle(handle);
	int ret = win_thread_map.erase(win_thread_id);
	ut_a(ret == 1);

	mutex_exit(&thread_mutex);

	ExitThread((DWORD) exit_value);
#else
	mutex_exit(&thread_mutex);
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
#if defined(_WIN32)
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
#ifdef _WIN32
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

