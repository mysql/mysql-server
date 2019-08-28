/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

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
#include "os0sync.h"

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
the function given. The start function takes a void* parameter
and returns an ulint.
@return	handle to the thread */
UNIV_INTERN
os_thread_t
os_thread_create_func(
/*==================*/
	os_thread_func_t	func,		/*!< in: pointer to function
						from which to start */
	void*			arg,		/*!< in: argument to start
						function */
	os_thread_id_t*		thread_id)	/*!< out: id of the created
						thread, or NULL */
{
	/* the new thread should look recent changes up here so far. */
	os_wmb;

#ifdef __WIN__
	os_thread_t	thread;
	DWORD		win_thread_id;

	os_mutex_enter(os_sync_mutex);
	os_thread_count++;
	os_mutex_exit(os_sync_mutex);

	thread = CreateThread(NULL,	/* no security attributes */
			      0,	/* default size stack */
			      func,
			      arg,
			      0,	/* thread runs immediately */
			      &win_thread_id);

	if (thread_id) {
		*thread_id = win_thread_id;
	}

	return(thread);
#else
	int		ret;
	os_thread_t	pthread;
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
		exit(1);
	}
#endif
	os_mutex_enter(os_sync_mutex);
	os_thread_count++;
	os_mutex_exit(os_sync_mutex);

#ifdef UNIV_HPUX10
	ret = pthread_create(&pthread, pthread_attr_default, func, arg);
#else
	ret = pthread_create(&pthread, &attr, func, arg);
#endif
	if (ret) {
		fprintf(stderr,
			"InnoDB: Error: pthread_create returned %d\n", ret);
		exit(1);
	}

#ifndef UNIV_HPUX10
	pthread_attr_destroy(&attr);
#endif

	ut_a(os_thread_count <= OS_THREAD_MAX_N);

	if (thread_id) {
		*thread_id = pthread;
	}

	return(pthread);
#endif
}

/** Waits until the specified thread completes and joins it.
Its return value is ignored.
@param[in,out]	thread	thread to join */
UNIV_INTERN
void
os_thread_join(
	os_thread_t	thread)
{
#ifdef __WIN__
	/* Do nothing. */
#else
#ifdef UNIV_DEBUG
	const int	ret =
#endif /* UNIV_DEBUG */
	pthread_join(thread, NULL);

	/* Waiting on already-quit threads is allowed. */
	ut_ad(ret == 0 || ret == ESRCH);
#endif /* __WIN__ */
}

/*****************************************************************//**
Exits the current thread. */
UNIV_INTERN
void
os_thread_exit(
/*===========*/
	void*	exit_value,	/*!< in: exit value; in Windows this void*
				is cast as a DWORD */
	bool	detach)		/*!< in: if true, the thread will be detached
				right before exiting. If false, another thread
				is responsible for joining this thread. */
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
	os_mutex_exit(os_sync_mutex);

#ifdef __WIN__
	ExitThread((DWORD) exit_value);
#else
	if (detach) {
		pthread_detach(pthread_self());
	}
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
