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
@file os/os0sync.cc
The interface to the operating system
synchronization primitives.

Created 9/6/1995 Heikki Tuuri
*******************************************************/

#include "os0sync.h"
#ifdef UNIV_NONINL
#include "os0sync.ic"
#endif

#ifdef __WIN__
#include <windows.h>
#endif

#include "ut0mem.h"
#include "srv0start.h"
#include "srv0srv.h"

/* Type definition for an operating system mutex struct */
struct os_mutex_t{
	os_event_t	event;	/*!< Used by sync0arr.cc for queing threads */
	void*		handle;	/*!< OS handle to mutex */
	ulint		count;	/*!< we use this counter to check
				that the same thread does not
				recursively lock the mutex: we
				do not assume that the OS mutex
				supports recursive locking, though
				NT seems to do that */
	UT_LIST_NODE_T(os_mutex_t) os_mutex_list;
				/* list of all 'slow' OS mutexes created */
};

/** Mutex protecting counts and the lists of OS mutexes and events */
UNIV_INTERN os_ib_mutex_t	os_sync_mutex;
/** TRUE if os_sync_mutex has been initialized */
static ibool		os_sync_mutex_inited	= FALSE;
/** TRUE when os_sync_free() is being executed */
static ibool		os_sync_free_called	= FALSE;

/** This is incremented by 1 in os_thread_create and decremented by 1 in
os_thread_exit */
UNIV_INTERN ulint	os_thread_count		= 0;

/** The list of all events created */
static UT_LIST_BASE_NODE_T(os_event)		os_event_list;

/** The list of all OS 'slow' mutexes */
static UT_LIST_BASE_NODE_T(os_mutex_t)		os_mutex_list;

UNIV_INTERN ulint	os_event_count		= 0;
UNIV_INTERN ulint	os_mutex_count		= 0;
UNIV_INTERN ulint	os_fast_mutex_count	= 0;

/* The number of microsecnds in a second. */
static const ulint MICROSECS_IN_A_SECOND = 1000000;

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	event_os_mutex_key;
UNIV_INTERN mysql_pfs_key_t	os_mutex_key;
#endif

/* Because a mutex is embedded inside an event and there is an
event embedded inside a mutex, on free, this generates a recursive call.
This version of the free event function doesn't acquire the global lock */
static void os_event_free_internal(os_event_t	event);

/* On Windows (Vista and later), load function pointers for condition
variable handling. Those functions are not available in prior versions,
so we have to use them via runtime loading, as long as we support XP. */
static void os_cond_module_init(void);

#ifdef __WIN__
/* Prototypes and function pointers for condition variable functions */
typedef VOID (WINAPI* InitializeConditionVariableProc)
	     (PCONDITION_VARIABLE ConditionVariable);
static InitializeConditionVariableProc initialize_condition_variable;

typedef BOOL (WINAPI* SleepConditionVariableCSProc)
	     (PCONDITION_VARIABLE ConditionVariable,
	      PCRITICAL_SECTION CriticalSection,
	      DWORD dwMilliseconds);
static SleepConditionVariableCSProc sleep_condition_variable;

typedef VOID (WINAPI* WakeAllConditionVariableProc)
	     (PCONDITION_VARIABLE ConditionVariable);
static WakeAllConditionVariableProc wake_all_condition_variable;

typedef VOID (WINAPI* WakeConditionVariableProc)
	     (PCONDITION_VARIABLE ConditionVariable);
static WakeConditionVariableProc wake_condition_variable;
#endif

/*********************************************************//**
Initialitze condition variable */
UNIV_INLINE
void
os_cond_init(
/*=========*/
	os_cond_t*	cond)	/*!< in: condition variable. */
{
	ut_a(cond);

#ifdef __WIN__
	ut_a(initialize_condition_variable != NULL);
	initialize_condition_variable(cond);
#else
	ut_a(pthread_cond_init(cond, NULL) == 0);
#endif
}

/*********************************************************//**
Do a timed wait on condition variable.
@return TRUE if timed out, FALSE otherwise */
UNIV_INLINE
ibool
os_cond_wait_timed(
/*===============*/
	os_cond_t*		cond,		/*!< in: condition variable. */
	os_fast_mutex_t*	fast_mutex,	/*!< in: fast mutex */
#ifndef __WIN__
	const struct timespec*	abstime		/*!< in: timeout */
#else
	DWORD			time_in_ms	/*!< in: timeout in
						milliseconds*/
#endif /* !__WIN__ */
)
{
	fast_mutex_t*	mutex = &fast_mutex->mutex;
#ifdef __WIN__
	BOOL	ret;
	DWORD	err;

	ut_a(sleep_condition_variable != NULL);

	ret = sleep_condition_variable(cond, mutex, time_in_ms);

	if (!ret) {
		err = GetLastError();
		/* From http://msdn.microsoft.com/en-us/library/ms686301%28VS.85%29.aspx,
		"Condition variables are subject to spurious wakeups
		(those not associated with an explicit wake) and stolen wakeups
		(another thread manages to run before the woken thread)."
		Check for both types of timeouts.
		Conditions are checked by the caller.*/
		if ((err == WAIT_TIMEOUT) || (err == ERROR_TIMEOUT)) {
			return(TRUE);
		}
	}

	ut_a(ret);

	return(FALSE);
#else
	int	ret;

	ret = pthread_cond_timedwait(cond, mutex, abstime);

	switch (ret) {
	case 0:
	case ETIMEDOUT:
	/* We play it safe by checking for EINTR even though
	according to the POSIX documentation it can't return EINTR. */
	case EINTR:
		break;

	default:
		fprintf(stderr, "  InnoDB: pthread_cond_timedwait() returned: "
				"%d: abstime={%lu,%lu}\n",
				ret, (ulong) abstime->tv_sec, (ulong) abstime->tv_nsec);
		ut_error;
	}

	return(ret == ETIMEDOUT);
#endif
}
/*********************************************************//**
Wait on condition variable */
UNIV_INLINE
void
os_cond_wait(
/*=========*/
	os_cond_t*		cond,	/*!< in: condition variable. */
	os_fast_mutex_t*	fast_mutex)/*!< in: fast mutex */
{
	fast_mutex_t*	mutex = &fast_mutex->mutex;
	ut_a(cond);
	ut_a(mutex);

#ifdef __WIN__
	ut_a(sleep_condition_variable != NULL);
	ut_a(sleep_condition_variable(cond, mutex, INFINITE));
#else
	ut_a(pthread_cond_wait(cond, mutex) == 0);
#endif
}

/*********************************************************//**
Wakes all threads  waiting for condition variable */
UNIV_INLINE
void
os_cond_broadcast(
/*==============*/
	os_cond_t*	cond)	/*!< in: condition variable. */
{
	ut_a(cond);

#ifdef __WIN__
	ut_a(wake_all_condition_variable != NULL);
	wake_all_condition_variable(cond);
#else
	ut_a(pthread_cond_broadcast(cond) == 0);
#endif
}

/*********************************************************//**
Wakes one thread waiting for condition variable */
UNIV_INLINE
void
os_cond_signal(
/*==========*/
	os_cond_t*	cond)	/*!< in: condition variable. */
{
	ut_a(cond);

#ifdef __WIN__
	ut_a(wake_condition_variable != NULL);
	wake_condition_variable(cond);
#else
	ut_a(pthread_cond_signal(cond) == 0);
#endif
}

/*********************************************************//**
Destroys condition variable */
UNIV_INLINE
void
os_cond_destroy(
/*============*/
	os_cond_t*	cond)	/*!< in: condition variable. */
{
#ifdef __WIN__
	/* Do nothing */
#else
	ut_a(pthread_cond_destroy(cond) == 0);
#endif
}

/*********************************************************//**
On Windows (Vista and later), load function pointers for condition variable
handling. Those functions are not available in prior versions, so we have to
use them via runtime loading, as long as we support XP. */
static
void
os_cond_module_init(void)
/*=====================*/
{
#ifdef __WIN__
	HMODULE		h_dll;

	if (!srv_use_native_conditions)
		return;

	h_dll = GetModuleHandle("kernel32");

	initialize_condition_variable = (InitializeConditionVariableProc)
			 GetProcAddress(h_dll, "InitializeConditionVariable");
	sleep_condition_variable = (SleepConditionVariableCSProc)
			  GetProcAddress(h_dll, "SleepConditionVariableCS");
	wake_all_condition_variable = (WakeAllConditionVariableProc)
			     GetProcAddress(h_dll, "WakeAllConditionVariable");
	wake_condition_variable = (WakeConditionVariableProc)
			 GetProcAddress(h_dll, "WakeConditionVariable");

	/* When using native condition variables, check function pointers */
	ut_a(initialize_condition_variable);
	ut_a(sleep_condition_variable);
	ut_a(wake_all_condition_variable);
	ut_a(wake_condition_variable);
#endif
}

/*********************************************************//**
Initializes global event and OS 'slow' mutex lists. */
UNIV_INTERN
void
os_sync_init(void)
/*==============*/
{
	UT_LIST_INIT(os_event_list);
	UT_LIST_INIT(os_mutex_list);

	os_sync_mutex = NULL;
	os_sync_mutex_inited = FALSE;

	/* Now for Windows only */
	os_cond_module_init();

	os_sync_mutex = os_mutex_create();

	os_sync_mutex_inited = TRUE;
}

/*********************************************************//**
Frees created events and OS 'slow' mutexes. */
UNIV_INTERN
void
os_sync_free(void)
/*==============*/
{
	os_event_t	event;
	os_ib_mutex_t	mutex;

	os_sync_free_called = TRUE;
	event = UT_LIST_GET_FIRST(os_event_list);

	while (event) {

		os_event_free(event);

		event = UT_LIST_GET_FIRST(os_event_list);
	}

	mutex = UT_LIST_GET_FIRST(os_mutex_list);

	while (mutex) {
		if (mutex == os_sync_mutex) {
			/* Set the flag to FALSE so that we do not try to
			reserve os_sync_mutex any more in remaining freeing
			operations in shutdown */
			os_sync_mutex_inited = FALSE;
		}

		os_mutex_free(mutex);

		mutex = UT_LIST_GET_FIRST(os_mutex_list);
	}
	os_sync_free_called = FALSE;
}

/*********************************************************//**
Creates an event semaphore, i.e., a semaphore which may just have two
states: signaled and nonsignaled. The created event is manual reset: it
must be reset explicitly by calling sync_os_reset_event.
@return	the event handle */
UNIV_INTERN
os_event_t
os_event_create(void)
/*==================*/
{
	os_event_t	event;

#ifdef __WIN__
	if(!srv_use_native_conditions) {

		event = static_cast<os_event_t>(ut_malloc(sizeof(*event)));

		event->handle = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!event->handle) {
			fprintf(stderr,
				"InnoDB: Could not create a Windows event"
				" semaphore; Windows error %lu\n",
				(ulong) GetLastError());
		}
	} else /* Windows with condition variables */
#endif
	{
		event = static_cast<os_event_t>(ut_malloc(sizeof *event));

#ifndef PFS_SKIP_EVENT_MUTEX
		os_fast_mutex_init(event_os_mutex_key, &event->os_mutex);
#else
		os_fast_mutex_init(PFS_NOT_INSTRUMENTED, &event->os_mutex);
#endif

		os_cond_init(&(event->cond_var));

		event->is_set = FALSE;

		/* We return this value in os_event_reset(), which can then be
		be used to pass to the os_event_wait_low(). The value of zero
		is reserved in os_event_wait_low() for the case when the
		caller does not want to pass any signal_count value. To
		distinguish between the two cases we initialize signal_count
		to 1 here. */
		event->signal_count = 1;
	}

	/* The os_sync_mutex can be NULL because during startup an event
	can be created [ because it's embedded in the mutex/rwlock ] before
	this module has been initialized */
	if (os_sync_mutex != NULL) {
		os_mutex_enter(os_sync_mutex);
	}

	/* Put to the list of events */
	UT_LIST_ADD_FIRST(os_event_list, os_event_list, event);

	os_event_count++;

	if (os_sync_mutex != NULL) {
		os_mutex_exit(os_sync_mutex);
	}

	return(event);
}

/**********************************************************//**
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
UNIV_INTERN
void
os_event_set(
/*=========*/
	os_event_t	event)	/*!< in: event to set */
{
	ut_a(event);

#ifdef __WIN__
	if (!srv_use_native_conditions) {
		ut_a(SetEvent(event->handle));
		return;
	}
#endif

	os_fast_mutex_lock(&(event->os_mutex));

	if (event->is_set) {
		/* Do nothing */
	} else {
		event->is_set = TRUE;
		event->signal_count += 1;
		os_cond_broadcast(&(event->cond_var));
	}

	os_fast_mutex_unlock(&(event->os_mutex));
}

/**********************************************************//**
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low().
@return	current signal_count. */
UNIV_INTERN
ib_int64_t
os_event_reset(
/*===========*/
	os_event_t	event)	/*!< in: event to reset */
{
	ib_int64_t	ret = 0;

	ut_a(event);

#ifdef __WIN__
	if(!srv_use_native_conditions) {
		ut_a(ResetEvent(event->handle));
		return(0);
	}
#endif

	os_fast_mutex_lock(&(event->os_mutex));

	if (!event->is_set) {
		/* Do nothing */
	} else {
		event->is_set = FALSE;
	}
	ret = event->signal_count;

	os_fast_mutex_unlock(&(event->os_mutex));
	return(ret);
}

/**********************************************************//**
Frees an event object, without acquiring the global lock. */
static
void
os_event_free_internal(
/*===================*/
	os_event_t	event)	/*!< in: event to free */
{
#ifdef __WIN__
	if(!srv_use_native_conditions) {
		ut_a(event);
		ut_a(CloseHandle(event->handle));
	} else
#endif
	{
		ut_a(event);

		/* This is to avoid freeing the mutex twice */
		os_fast_mutex_free(&(event->os_mutex));

		os_cond_destroy(&(event->cond_var));
	}

	/* Remove from the list of events */
	UT_LIST_REMOVE(os_event_list, os_event_list, event);

	os_event_count--;

	ut_free(event);
}

/**********************************************************//**
Frees an event object. */
UNIV_INTERN
void
os_event_free(
/*==========*/
	os_event_t	event)	/*!< in: event to free */

{
	ut_a(event);
#ifdef __WIN__
	if(!srv_use_native_conditions){
		ut_a(CloseHandle(event->handle));
	} else /*Windows with condition variables */
#endif
	{
		os_fast_mutex_free(&(event->os_mutex));

		os_cond_destroy(&(event->cond_var));
	}

	/* Remove from the list of events */
	os_mutex_enter(os_sync_mutex);

	UT_LIST_REMOVE(os_event_list, os_event_list, event);

	os_event_count--;

	os_mutex_exit(os_sync_mutex);

	ut_free(event);
}

/**********************************************************//**
Waits for an event object until it is in the signaled state.

Typically, if the event has been signalled after the os_event_reset()
we'll return immediately because event->is_set == TRUE.
There are, however, situations (e.g.: sync_array code) where we may
lose this information. For example:

thread A calls os_event_reset()
thread B calls os_event_set()   [event->is_set == TRUE]
thread C calls os_event_reset() [event->is_set == FALSE]
thread A calls os_event_wait()  [infinite wait!]
thread C calls os_event_wait()  [infinite wait!]

Where such a scenario is possible, to avoid infinite wait, the
value returned by os_event_reset() should be passed in as
reset_sig_count. */
UNIV_INTERN
void
os_event_wait_low(
/*==============*/
	os_event_t	event,		/*!< in: event to wait */
	ib_int64_t	reset_sig_count)/*!< in: zero or the value
					returned by previous call of
					os_event_reset(). */
{
#ifdef __WIN__
	if(!srv_use_native_conditions) {
		DWORD	err;

		ut_a(event);

		UT_NOT_USED(reset_sig_count);

		/* Specify an infinite wait */
		err = WaitForSingleObject(event->handle, INFINITE);

		ut_a(err == WAIT_OBJECT_0);
		return;
	}
#endif

	os_fast_mutex_lock(&event->os_mutex);

	if (!reset_sig_count) {
		reset_sig_count = event->signal_count;
	}

	while (!event->is_set && event->signal_count == reset_sig_count) {
		os_cond_wait(&(event->cond_var), &(event->os_mutex));

		/* Solaris manual said that spurious wakeups may occur: we
		have to check if the event really has been signaled after
		we came here to wait */
	}

	os_fast_mutex_unlock(&event->os_mutex);
}

/**********************************************************//**
Waits for an event object until it is in the signaled state or
a timeout is exceeded.
@return	0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
UNIV_INTERN
ulint
os_event_wait_time_low(
/*===================*/
	os_event_t	event,			/*!< in: event to wait */
	ulint		time_in_usec,		/*!< in: timeout in
						microseconds, or
						OS_SYNC_INFINITE_TIME */
	ib_int64_t	reset_sig_count)	/*!< in: zero or the value
						returned by previous call of
						os_event_reset(). */
{
	ibool		timed_out = FALSE;

#ifdef __WIN__
	DWORD		time_in_ms;

	if (!srv_use_native_conditions) {
		DWORD	err;

		ut_a(event);

		if (time_in_usec != OS_SYNC_INFINITE_TIME) {
			time_in_ms = static_cast<DWORD>(time_in_usec / 1000);
			err = WaitForSingleObject(event->handle, time_in_ms);
		} else {
			err = WaitForSingleObject(event->handle, INFINITE);
		}

		if (err == WAIT_OBJECT_0) {
			return(0);
		} else if ((err == WAIT_TIMEOUT) || (err == ERROR_TIMEOUT)) {
			return(OS_SYNC_TIME_EXCEEDED);
		}

		ut_error;
		/* Dummy value to eliminate compiler warning. */
		return(42);
	} else {
		ut_a(sleep_condition_variable != NULL);

		if (time_in_usec != OS_SYNC_INFINITE_TIME) {
			time_in_ms = static_cast<DWORD>(time_in_usec / 1000);
		} else {
			time_in_ms = INFINITE;
		}
	}
#else
	struct timespec	abstime;

	if (time_in_usec != OS_SYNC_INFINITE_TIME) {
		struct timeval	tv;
		int		ret;
		ulint		sec;
		ulint		usec;

		ret = ut_usectime(&sec, &usec);
		ut_a(ret == 0);

		tv.tv_sec = sec;
		tv.tv_usec = usec;

		tv.tv_usec += time_in_usec;

		if ((ulint) tv.tv_usec >= MICROSECS_IN_A_SECOND) {
			tv.tv_sec += time_in_usec / MICROSECS_IN_A_SECOND;
			tv.tv_usec %= MICROSECS_IN_A_SECOND;
		}

		abstime.tv_sec  = tv.tv_sec;
		abstime.tv_nsec = tv.tv_usec * 1000;
	} else {
		abstime.tv_nsec = 999999999;
		abstime.tv_sec = (time_t) ULINT_MAX;
	}

	ut_a(abstime.tv_nsec <= 999999999);

#endif /* __WIN__ */

	os_fast_mutex_lock(&event->os_mutex);

	if (!reset_sig_count) {
		reset_sig_count = event->signal_count;
	}

	do {
		if (event->is_set || event->signal_count != reset_sig_count) {

			break;
		}

		timed_out = os_cond_wait_timed(
			&event->cond_var, &event->os_mutex,
#ifndef __WIN__
			&abstime
#else
			time_in_ms
#endif /* !__WIN__ */
		);

	} while (!timed_out);

	os_fast_mutex_unlock(&event->os_mutex);

	return(timed_out ? OS_SYNC_TIME_EXCEEDED : 0);
}

/*********************************************************//**
Creates an operating system mutex semaphore. Because these are slow, the
mutex semaphore of InnoDB itself (ib_mutex_t) should be used where possible.
@return	the mutex handle */
UNIV_INTERN
os_ib_mutex_t
os_mutex_create(void)
/*=================*/
{
	os_fast_mutex_t*	mutex;
	os_ib_mutex_t		mutex_str;

	mutex = static_cast<os_fast_mutex_t*>(
		ut_malloc(sizeof(os_fast_mutex_t)));

	os_fast_mutex_init(os_mutex_key, mutex);

	mutex_str = static_cast<os_ib_mutex_t>(ut_malloc(sizeof *mutex_str));

	mutex_str->handle = mutex;
	mutex_str->count = 0;
	mutex_str->event = os_event_create();

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		/* When creating os_sync_mutex itself we cannot reserve it */
		os_mutex_enter(os_sync_mutex);
	}

	UT_LIST_ADD_FIRST(os_mutex_list, os_mutex_list, mutex_str);

	os_mutex_count++;

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		os_mutex_exit(os_sync_mutex);
	}

	return(mutex_str);
}

/**********************************************************//**
Acquires ownership of a mutex semaphore. */
UNIV_INTERN
void
os_mutex_enter(
/*===========*/
	os_ib_mutex_t	mutex)	/*!< in: mutex to acquire */
{
	os_fast_mutex_lock(static_cast<os_fast_mutex_t*>(mutex->handle));

	(mutex->count)++;

	ut_a(mutex->count == 1);
}

/**********************************************************//**
Releases ownership of a mutex. */
UNIV_INTERN
void
os_mutex_exit(
/*==========*/
	os_ib_mutex_t	mutex)	/*!< in: mutex to release */
{
	ut_a(mutex);

	ut_a(mutex->count == 1);

	(mutex->count)--;
	os_fast_mutex_unlock(static_cast<os_fast_mutex_t*>(mutex->handle));
}

/**********************************************************//**
Frees a mutex object. */
UNIV_INTERN
void
os_mutex_free(
/*==========*/
	os_ib_mutex_t	mutex)	/*!< in: mutex to free */
{
	ut_a(mutex);

	if (UNIV_LIKELY(!os_sync_free_called)) {
		os_event_free_internal(mutex->event);
	}

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		os_mutex_enter(os_sync_mutex);
	}

	UT_LIST_REMOVE(os_mutex_list, os_mutex_list, mutex);

	os_mutex_count--;

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		os_mutex_exit(os_sync_mutex);
	}

	os_fast_mutex_free(static_cast<os_fast_mutex_t*>(mutex->handle));
	ut_free(mutex->handle);
	ut_free(mutex);
}

/*********************************************************//**
Initializes an operating system fast mutex semaphore. */
UNIV_INTERN
void
os_fast_mutex_init_func(
/*====================*/
	fast_mutex_t*		fast_mutex)	/*!< in: fast mutex */
{
#ifdef __WIN__
	ut_a(fast_mutex);

	InitializeCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	ut_a(0 == pthread_mutex_init(fast_mutex, MY_MUTEX_INIT_FAST));
#endif
	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		/* When creating os_sync_mutex itself (in Unix) we cannot
		reserve it */

		os_mutex_enter(os_sync_mutex);
	}

	os_fast_mutex_count++;

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		os_mutex_exit(os_sync_mutex);
	}
}

/**********************************************************//**
Acquires ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_lock_func(
/*====================*/
	fast_mutex_t*		fast_mutex)	/*!< in: mutex to acquire */
{
#ifdef __WIN__
	EnterCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	pthread_mutex_lock(fast_mutex);
#endif
}

/**********************************************************//**
Releases ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_unlock_func(
/*======================*/
	fast_mutex_t*		fast_mutex)	/*!< in: mutex to release */
{
#ifdef __WIN__
	LeaveCriticalSection(fast_mutex);
#else
	pthread_mutex_unlock(fast_mutex);
#endif
}

/**********************************************************//**
Frees a mutex object. */
UNIV_INTERN
void
os_fast_mutex_free_func(
/*====================*/
	fast_mutex_t*		fast_mutex)	/*!< in: mutex to free */
{
#ifdef __WIN__
	ut_a(fast_mutex);

	DeleteCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	int	ret;

	ret = pthread_mutex_destroy(fast_mutex);

	if (UNIV_UNLIKELY(ret != 0)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: error: return value %lu when calling\n"
			"InnoDB: pthread_mutex_destroy().\n", (ulint) ret);
		fprintf(stderr,
			"InnoDB: Byte contents of the pthread mutex at %p:\n",
			(void*) fast_mutex);
		ut_print_buf(stderr, fast_mutex, sizeof(os_fast_mutex_t));
		putc('\n', stderr);
	}
#endif
	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		/* When freeing the last mutexes, we have
		already freed os_sync_mutex */

		os_mutex_enter(os_sync_mutex);
	}

	ut_ad(os_fast_mutex_count > 0);
	os_fast_mutex_count--;

	if (UNIV_LIKELY(os_sync_mutex_inited)) {
		os_mutex_exit(os_sync_mutex);
	}
}
