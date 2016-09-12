/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
#include "ut0new.h"

#ifdef UNIV_NONINL
#include "os0thread.ic"
#endif

#ifndef UNIV_HOTBACKUP
#include "srv0srv.h"
#include "os0event.h"

#include <map>

/** Mutex that tracks the thread count. Used by innorwlocktest.cc
FIXME: the unit tests should use APIs */
SysMutex	thread_mutex;

/** Number of threads active. */
ulint	os_thread_count;

#ifdef _WIN32
typedef std::map<
	DWORD,
	HANDLE,
	std::less<DWORD>,
	ut_allocator<std::pair<const DWORD, HANDLE> > >	WinThreadMap;
/** This STL map remembers the initial handle returned by CreateThread
so that it can be closed when the thread exits. */
static WinThreadMap	win_thread_map;
#endif /* _WIN32 */

/***************************************************************//**
Compares two thread ids for equality.
@return TRUE if equal */
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
@return thread identifier as a number */
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
@return current thread identifier */
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

	/* the new thread should look recent changes up here so far. */
	os_wmb;

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
		ib::fatal() << "CreateThread returned " << GetLastError();
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
		ib::fatal() << "pthread_create returned " << ret;
	}

	pthread_attr_destroy(&attr);

#endif /* not _WIN32 */

	ut_a(os_thread_count <= OS_THREAD_MAX_N);

	/* Return the thread_id if the caller requests it. */
	if (thread_id != NULL) {
		*thread_id = new_thread_id;
	}
}

/** Waits until the specified thread completes and joins it.
Its return value is ignored.
@param[in,out]	thread	thread to join */
void
os_thread_join(
	os_thread_id_t	thread)
{
#ifdef _WIN32
	/* Do nothing. */
#else
#ifdef UNIV_DEBUG
	const int	ret =
#endif /* UNIV_DEBUG */
	pthread_join(thread, NULL);

	/* Waiting on already-quit threads is allowed. */
	ut_ad(ret == 0 || ret == ESRCH);
#endif /* _WIN32 */
}

/** Exits the current thread.
@param[in]	detach	if true, the thread will be detached right before
exiting. If false, another thread is responsible for joining this thread */
void
os_thread_exit(
	bool	detach)
{
#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Thread exits, id "
		<< os_thread_pf(os_thread_get_curr_id());
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
	size_t ret = win_thread_map.erase(win_thread_id);
	ut_a(ret == 1);

	mutex_exit(&thread_mutex);

	ExitThread(0);
#else
	mutex_exit(&thread_mutex);
	if (detach) {
		pthread_detach(pthread_self());
	}
	pthread_exit(NULL);
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
#else
	sched_yield();
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
#elif defined(HAVE_NANOSLEEP)
	struct timespec	t;

	t.tv_sec = tm / 1000000;
	t.tv_nsec = (tm % 1000000) * 1000;

	::nanosleep(&t, NULL);
#else
	struct timeval  t;

	t.tv_sec = tm / 1000000;
	t.tv_usec = tm % 1000000;

	select(0, NULL, NULL, NULL, &t);
#endif /* _WIN32 */
}

/*****************************************************************//**
Check if there are threads active.
@return true if the thread count > 0. */
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
void
os_thread_init()
/*============*/
{
	mutex_create(LATCH_ID_THREAD_MUTEX, &thread_mutex);
}

/**
Frees OS thread management data structures. */
void
os_thread_free()
/*============*/
{
	if (os_thread_count != 0) {
		ib::warn() << "Some (" << os_thread_count << ") threads are"
			" still active";
	}

	mutex_destroy(&thread_mutex);
}

