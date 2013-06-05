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

#include "ha_prototypes.h"

#include "os0thread.h"
#ifdef UNIV_NONINL
#include "os0thread.ic"
#endif

#ifndef UNIV_HOTBACKUP
#include "srv0srv.h"

#ifdef _WIN32
/** This STL map remembers the initial handle returned by CreateThread
so that it can be closed when the thread exits. */
static std::map<DWORD, HANDLE>	win_thread_map;
#endif /* _WIN32 */

/***************************************************************//**
Compares two thread ids for equality.
@return	TRUE if equal */

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

	os_mutex_enter(os_sync_mutex);

	handle = CreateThread(NULL,	/* no security attributes */
			      0,	/* default size stack */
			      func,
			      arg,
			      0,	/* thread runs immediately */
			      &new_thread_id);

	if (!handle) {
		os_mutex_exit(os_sync_mutex);
		/* If we cannot start a new thread, life has no meaning. */
		ib_logf(IB_LOG_LEVEL_FATAL,
			"CreateThread returned %d", GetLastError());
	}

	std::pair<std::map<DWORD, HANDLE>::iterator,bool> ret;
	ret = win_thread_map.insert(
		std::pair<DWORD, HANDLE>(new_thread_id, handle));
	ut_ad((*ret.first).first == new_thread_id);
	ut_ad((*ret.first).second == handle);
	ut_a(ret.second == true);	/* true means thread_id was new */
	os_thread_count++;

	os_mutex_exit(os_sync_mutex);

#else /* _WIN32 else */

	int		ret;
	pthread_attr_t	attr;

	pthread_attr_init(&attr);

	os_mutex_enter(os_sync_mutex);
	os_thread_count++;
	os_mutex_exit(os_sync_mutex);

	ret = pthread_create(&new_thread_id, &attr, func, arg);

	if (ret) {
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

	os_mutex_enter(os_sync_mutex);
	os_thread_count--;

#ifdef _WIN32
	DWORD win_thread_id = GetCurrentThreadId();
	HANDLE handle = win_thread_map[win_thread_id];
	CloseHandle(handle);
	int ret = win_thread_map.erase(win_thread_id);
	ut_a(ret == 1);

	os_mutex_exit(os_sync_mutex);

	ExitThread((DWORD) exit_value);
#else
	os_mutex_exit(os_sync_mutex);
	pthread_detach(pthread_self());
	pthread_exit(exit_value);
#endif
}

/*****************************************************************//**
Advises the os to give up remainder of the thread's time slice. */

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
