/******************************************************
The interface to the operating system
process and thread control primitives

(c) 1995 Innobase Oy

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#include "os0thread.h"
#ifdef UNIV_NONINL
#include "os0thread.ic"
#endif

#ifdef __WIN__
#include <windows.h>
#endif

#include "srv0srv.h"

/*******************************************************************
Compares two threads or thread ids for equality */

ibool
os_thread_eq(
/*=========*/
				/* out: TRUE if equal */
	os_thread_t	a,	/* in: OS thread or thread id */
	os_thread_t	b)	/* in: OS thread or thread id */
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

/********************************************************************
Converts an OS thread or thread id to a ulint. It is NOT guaranteed that
the ulint is unique for the thread though! */

ulint
os_thread_pf(
/*=========*/
	os_thread_t	a)
{
#ifdef UNIV_HPUX
        /* In HP-UX a pthread_t is a struct of 3 fields: field1, field2,
        field3. We do not know if field1 determines the thread uniquely. */

	return((ulint)(a.field1));
#else
	return((ulint)a);
#endif
}

/*********************************************************************
Returns the thread identifier of current thread. Currently the thread
identifier is the thread handle itself. Note that in HP-UX pthread_t is
a struct of 3 fields. */

os_thread_id_t
os_thread_get_curr_id(void)
/*=======================*/
{
#ifdef __WIN__
	return(GetCurrentThread());
#else
	return(pthread_self());
#endif
}

/********************************************************************
Creates a new thread of execution. The execution starts from
the function given. The start function takes a void* parameter
and returns an ulint. */

os_thread_t
os_thread_create(
/*=============*/
						/* out: handle to the thread */
#ifndef __WIN__
		 os_posix_f_t            start_f,
#else
	ulint (*start_f)(void*),		/* in: pointer to function
						from which to start */
#endif
	void*			arg,		/* in: argument to start
						function */
	os_thread_id_t*		thread_id)	/* out: id of created
						thread; currently this is
						identical to the handle to
						the thread */
{
#ifdef __WIN__
	os_thread_t	thread;
	ulint           win_thread_id;

	thread = CreateThread(NULL,	/* no security attributes */
				0,	/* default size stack */
				(LPTHREAD_START_ROUTINE)start_f,
				arg,
				0,	/* thread runs immediately */
				&win_thread_id);

	if (srv_set_thread_priorities) {

	        /* Set created thread priority the same as a normal query
	        in MYSQL: we try to prevent starvation of threads by
	        assigning same priority QUERY_PRIOR to all */

	        ut_a(SetThreadPriority(thread, srv_query_thread_priority));
	}

	*thread_id = thread;

	return(thread);
#else
	int		ret;
	os_thread_t	pthread;
	pthread_attr_t  attr;

        pthread_attr_init(&attr);

	ret = pthread_create(&pthread, &attr, start_f, arg);

	pthread_attr_destroy(&attr);

	if (srv_set_thread_priorities) {
	
	        my_pthread_setprio(pthread, srv_query_thread_priority);
	}

	*thread_id = pthread;

	return(pthread);
#endif
}

/*********************************************************************
Returns handle to the current thread. */

os_thread_t
os_thread_get_curr(void)
/*====================*/
{
#ifdef __WIN__
	return(GetCurrentThread());
#else
	return(pthread_self());
#endif
}
	
/*********************************************************************
Advises the os to give up remainder of the thread's time slice. */

void
os_thread_yield(void)
/*=================*/
{
#if defined(__WIN__)
	Sleep(0);
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

/*********************************************************************
The thread sleeps at least the time given in microseconds. */

void
os_thread_sleep(
/*============*/
	ulint	tm)	/* in: time in microseconds */
{
#ifdef __WIN__
	Sleep(tm / 1000);
#else
	struct timeval	t;

	t.tv_sec = tm / 1000000;
	t.tv_usec = tm % 1000000;
	
	select(0, NULL, NULL, NULL, &t);
#endif
}

/**********************************************************************
Sets a thread priority. */

void
os_thread_set_priority(
/*===================*/
	os_thread_t	handle,	/* in: OS handle to the thread */
	ulint		pri)	/* in: priority */
{
#ifdef __WIN__
	int	os_pri;

	if (pri == OS_THREAD_PRIORITY_BACKGROUND) {
		os_pri = THREAD_PRIORITY_BELOW_NORMAL;
	} else if (pri == OS_THREAD_PRIORITY_NORMAL) {
		os_pri = THREAD_PRIORITY_NORMAL;
	} else if (pri == OS_THREAD_PRIORITY_ABOVE_NORMAL) {
		os_pri = THREAD_PRIORITY_HIGHEST;
	} else {
		ut_error;
	}

	ut_a(SetThreadPriority(handle, os_pri));
#else
	UT_NOT_USED(handle);
	UT_NOT_USED(pri);
#endif
}

/**********************************************************************
Gets a thread priority. */

ulint
os_thread_get_priority(
/*===================*/
				/* out: priority */
	os_thread_t	handle)	/* in: OS handle to the thread */
{
#ifdef __WIN__
	int	os_pri;
	ulint	pri;

	os_pri = GetThreadPriority(handle);

	if (os_pri == THREAD_PRIORITY_BELOW_NORMAL) {
		pri = OS_THREAD_PRIORITY_BACKGROUND;
	} else if (os_pri == THREAD_PRIORITY_NORMAL) {
		pri = OS_THREAD_PRIORITY_NORMAL;
	} else if (os_pri == THREAD_PRIORITY_HIGHEST) {
		pri = OS_THREAD_PRIORITY_ABOVE_NORMAL;
	} else {
		ut_error;
	}

	return(pri);
#else
	return(0);
#endif
}

/**********************************************************************
Gets the last operating system error code for the calling thread. */

ulint
os_thread_get_last_error(void)
/*==========================*/
{
#ifdef __WIN__
	return(GetLastError());
#else
	return(0);
#endif
}
