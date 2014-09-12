/*****************************************************************************

Copyright (c) 2013, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file os/os0event.cc
The interface to the operating system condition variables.

Created 2012-09-23 Sunny Bains
*******************************************************/

#include "os0event.h"
#include "ut0mutex.h"
#include "ha_prototypes.h"
#include "ut0new.h"

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

#include <list>

/** The number of microsecnds in a second. */
static const ulint MICROSECS_IN_A_SECOND = 1000000;

#ifdef _WIN32
/** Native event (slow) */
typedef HANDLE			os_win_event_t;

/** For Windows native condition variables. We use runtime loading / function
pointers, because they are not available on Windows Server 2003 and
Windows XP/2000.

We use condition for events on Windows if possible, even if os_event
resembles Windows kernel event object well API-wise. The reason is
performance, kernel objects are heavyweights and WaitForSingleObject() is a
performance killer causing calling thread to context switch. Besides, InnoDB
is preallocating large number (often millions) of os_events. With kernel event
objects it takes a big chunk out of non-paged pool, which is better suited
for tasks like IO than for storing idle event objects. */
bool	srv_use_native_conditions;

/** Native condition variable. */
typedef CONDITION_VARIABLE	os_cond_t;

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
#else
/** Native condition variable */
typedef pthread_cond_t		os_cond_t;
#endif /* _WIN32 */

typedef std::list<os_event_t, ut_allocator<os_event_t> >	os_event_list_t;
typedef os_event_list_t::iterator				event_iter_t;

/** InnoDB condition variable. */
struct os_event {
	os_event(const char* name) UNIV_NOTHROW;

	~os_event() UNIV_NOTHROW;

	/**
	Destroys a condition variable */
	void destroy() UNIV_NOTHROW
	{
#ifdef _WIN32
		/* Do nothing */
#else
		{
			int	ret;

			ret = pthread_cond_destroy(&cond_var);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/** Set the event */
	void set() UNIV_NOTHROW
	{
#ifdef _WIN32
		if (!srv_use_native_conditions) {
			ut_a(SetEvent(handle));
		}
#endif /* _WIN32 */

		mutex_enter(&mutex);

		if (!m_set) {
			broadcast();
		}

		mutex_exit(&mutex);
	}

	int64_t reset() UNIV_NOTHROW
	{
#ifdef _WIN32
		if (!srv_use_native_conditions) {
			ut_a(ResetEvent(handle));
			return(0);
		}
#endif /* _WIN32 */
		mutex_enter(&mutex);

		if (m_set) {
			m_set = false;
		}

		int64_t	ret = signal_count;

		mutex_exit(&mutex);

		return(ret);
	}

	/**
	Waits for an event object until it is in the signaled state.

	Typically, if the event has been signalled after the os_event_reset()
	we'll return immediately because event->m_set == true.
	There are, however, situations (e.g.: sync_array code) where we may
	lose this information. For example:

	thread A calls os_event_reset()
	thread B calls os_event_set()   [event->m_set == true]
	thread C calls os_event_reset() [event->m_set == false]
	thread A calls os_event_wait()  [infinite wait!]
	thread C calls os_event_wait()  [infinite wait!]

	Where such a scenario is possible, to avoid infinite wait, the
	value returned by reset() should be passed in as
	reset_sig_count. */
	void wait_low(int64_t reset_sig_count) UNIV_NOTHROW;

	/**
	Waits for an event object until it is in the signaled state or
	a timeout is exceeded.
	@param time_in_usec - timeout in microseconds,
			or OS_SYNC_INFINITE_TIME
	@param reset_sig_count- zero or the value returned by
			previous call of os_event_reset().
	@return	0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
	ulint wait_time_low(
		ulint		time_in_usec,
		int64_t		reset_sig_count) UNIV_NOTHROW;

	/** @return true if the event is in the signalled state. */
	bool is_set() const UNIV_NOTHROW
	{
		return(m_set);
	}

private:
	/**
	Initialize a condition variable */
	void init() UNIV_NOTHROW
	{
#ifdef _WIN32
		ut_a(initialize_condition_variable != NULL);
		initialize_condition_variable(&cond_var);
#else
		{
			int	ret;

			ret = pthread_cond_init(&cond_var, NULL);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/**
	Wait on condition variable */
	void wait() UNIV_NOTHROW
	{
#ifdef _WIN32
		ut_a(sleep_condition_variable != NULL);
		ut_a(sleep_condition_variable(&cond_var, mutex, INFINITE));
#else
		{
			int	ret;

			ret = pthread_cond_wait(&cond_var, mutex);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/**
	Wakes all threads waiting for condition variable */
	void broadcast() UNIV_NOTHROW
	{
		m_set = true;
		++signal_count;

#ifdef _WIN32
		ut_a(wake_all_condition_variable != NULL);
		wake_all_condition_variable(&cond_var);
#else
		{
			int	ret;

			ret = pthread_cond_broadcast(&cond_var);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/**
	Wakes one thread waiting for condition variable */
	void signal() UNIV_NOTHROW
	{
#ifdef _WIN32
		ut_a(wake_condition_variable != NULL);
		wake_condition_variable(&cond_var);
#else
		{
			int	ret;

			ret = pthread_cond_signal(&cond_var);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/**
	Do a timed wait on condition variable.
	@param abstime - timeout
	@param time_in_ms - timeout in milliseconds.
	@return true if timed out, false otherwise */
	bool timed_wait(
#ifndef _WIN32
		const timespec*	abstime
#else
		DWORD		time_in_ms
#endif /* !_WIN32 */
	);

private:
#ifdef _WIN32
	os_win_event_t		handle;		/*!< kernel event object, slow,
						used on older Windows */
#endif /* _WIN32 */
	EventMutex		mutex;		/*!< this mutex protects
						the next fields */

	bool			m_set;		/*!< this is true when the
						event is in the signaled
						state, i.e., a thread does
						not stop if it tries to wait
						for this event */

	int64_t			signal_count;	/*!< this is incremented
						each time the event becomes
						signaled */

	os_cond_t		cond_var;	/*!< condition variable is
						used in waiting for the event */

public:
	event_iter_t		event_iter;	/*!< For O(1) removal from
						list */
protected:
	// Disable copying
	os_event(const os_event&);
	os_event& operator=(const os_event&);
};

#ifdef _WIN32
/**
On Windows (Vista and later), load function pointers for condition variable
handling. Those functions are not available in prior versions, so we have to
use them via runtime loading, as long as we support XP. */
static
void
os_win_init()
/*=========*/
{
	if (!srv_use_native_conditions) {
		return;
	}

	HMODULE		h_dll = GetModuleHandle("kernel32");

	initialize_condition_variable = (InitializeConditionVariableProc)
		GetProcAddress(h_dll, "InitializeConditionVariable");

	sleep_condition_variable = (SleepConditionVariableCSProc)
		GetProcAddress(h_dll, "SleepConditionVariableCS");

	wake_all_condition_variable = (WakeAllConditionVariableProc)
		GetProcAddress(h_dll, "WakeAllConditionVariable");

	wake_condition_variable = (WakeConditionVariableProc)
		GetProcAddress(h_dll, "WakeConditionVariable");

	/* When using native condition variables, check function pointers */
	ut_a(wake_condition_variable);
	ut_a(sleep_condition_variable);
	ut_a(wake_all_condition_variable);
	ut_a(initialize_condition_variable);
}
#endif /* _WIN32 */

/**
Do a timed wait on condition variable.
@param abstime - absolute time to wait
@param time_in_ms - timeout in milliseconds
@return true if timed out */
bool
os_event::timed_wait(
#ifndef _WIN32
	const timespec*	abstime
#else
	DWORD		time_in_ms
#endif /* !_WIN32 */
)
{
#ifdef _WIN32
	ut_a(sleep_condition_variable != NULL);

	BOOL		ret;

	ret = sleep_condition_variable(&cond_var, mutex, time_in_ms);

	if (!ret) {
		DWORD	err = GetLastError();

		/* FQDN=msdn.microsoft.com
		@see http://$FQDN/en-us/library/ms686301%28VS.85%29.aspx,

		"Condition variables are subject to spurious wakeups
		(those not associated with an explicit wake) and stolen wakeups
		(another thread manages to run before the woken thread)."
		Check for both types of timeouts.
		Conditions are checked by the caller.*/
		if (err == WAIT_TIMEOUT || err == ERROR_TIMEOUT) {
			return(true);
		}
	}

	ut_a(ret);

	return(false);
#else
	int	ret;

	ret = pthread_cond_timedwait(&cond_var, mutex, abstime);

	switch (ret) {
	case 0:
	case ETIMEDOUT:
		/* We play it safe by checking for EINTR even though
		according to the POSIX documentation it can't return EINTR. */
	case EINTR:
		break;

	default:
		ib::error() << "pthread_cond_timedwait() returned: " << ret
			<< ": abstime={" << abstime->tv_sec << ","
			<< abstime->tv_nsec << "}";
		ut_error;
	}

	return(ret == ETIMEDOUT);
#endif /* _WIN32 */
}

/**
Waits for an event object until it is in the signaled state.

Typically, if the event has been signalled after the os_event_reset()
we'll return immediately because event->m_set == true.
There are, however, situations (e.g.: sync_array code) where we may
lose this information. For example:

thread A calls os_event_reset()
thread B calls os_event_set()   [event->m_set == true]
thread C calls os_event_reset() [event->m_set == false]
thread A calls os_event_wait()  [infinite wait!]
thread C calls os_event_wait()  [infinite wait!]

Where such a scenario is possible, to avoid infinite wait, the
value returned by reset() should be passed in as
reset_sig_count. */
void
os_event::wait_low(
	int64_t		reset_sig_count) UNIV_NOTHROW
{
#ifdef _WIN32
	if (!srv_use_native_conditions) {
		DWORD	err;

		/* Specify an infinite wait */
		err = WaitForSingleObject(handle, INFINITE);
		ut_a(err == WAIT_OBJECT_0);

		return;
	}
#endif /* _WIN32 */

	mutex_enter(&mutex);

	if (!reset_sig_count) {
		reset_sig_count = signal_count;
	}

	while (!m_set && signal_count == reset_sig_count) {

		wait();

		/* Spurious wakeups may occur: we have to check if the
		event really has been signaled after we came here to wait. */
	}

	mutex_exit(&mutex);
}

/**
Waits for an event object until it is in the signaled state or
a timeout is exceeded.
@param time_in_usec - timeout in microseconds, or OS_SYNC_INFINITE_TIME
@param reset_sig_count - zero or the value returned by previous call
	of os_event_reset().
@return	0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
ulint
os_event::wait_time_low(
	ulint		time_in_usec,
	int64_t		reset_sig_count) UNIV_NOTHROW
{
	bool		timed_out = false;

#ifdef _WIN32
	DWORD		time_in_ms;

	if (!srv_use_native_conditions) {
		DWORD	err;

		if (time_in_usec != OS_SYNC_INFINITE_TIME) {
			time_in_ms = DWORD(time_in_usec / 1000);
			err = WaitForSingleObject(handle, time_in_ms);
		} else {
			err = WaitForSingleObject(handle, INFINITE);
		}

		if (err == WAIT_OBJECT_0) {
			return(0);
		} else if (err == WAIT_TIMEOUT || err == ERROR_TIMEOUT) {
			return(OS_SYNC_TIME_EXCEEDED);
		}

		ut_error;
		/* Dummy value to eliminate compiler warning. */
		return(42);
	} else {
		ut_a(sleep_condition_variable != NULL);

		if (time_in_usec != OS_SYNC_INFINITE_TIME) {
			time_in_ms = DWORD(time_in_usec / 1000);
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
			tv.tv_sec += tv.tv_usec / MICROSECS_IN_A_SECOND;
			tv.tv_usec %= MICROSECS_IN_A_SECOND;
		}

		abstime.tv_sec  = tv.tv_sec;
		abstime.tv_nsec = tv.tv_usec * 1000;
	} else {
		abstime.tv_nsec = 999999999;
		abstime.tv_sec = (time_t) ULINT_MAX;
	}

	ut_a(abstime.tv_nsec <= 999999999);

#endif /* _WIN32 */

	mutex_enter(&mutex);

	if (!reset_sig_count) {
		reset_sig_count = signal_count;
	}

	do {
		if (m_set || signal_count != reset_sig_count) {

			break;
		}

#ifndef _WIN32
		timed_out = timed_wait(&abstime);
#else
		timed_out = timed_wait(time_in_ms);
#endif /* !_WIN32 */

	} while (!timed_out);

	mutex_exit(&mutex);

	return(timed_out ? OS_SYNC_TIME_EXCEEDED : 0);
}

/** Constructor */
os_event::os_event(const char* name) UNIV_NOTHROW
{
#ifdef _WIN32
	if (!srv_use_native_conditions) {

		handle = CreateEvent(0, TRUE, FALSE, (LPCTSTR) name);

		if (!handle) {
			ib::error() << "Could not create a Windows event"
				" semaphore; Windows error " << GetLastError();
		}

	} else /* Windows with condition variables */
#endif /* _WIN32 */
	{
		mutex_create("event_mutex", &mutex);

		init();

		m_set = false;

		/* We return this value in os_event_reset(),
		which can then be be used to pass to the
		os_event_wait_low(). The value of zero is
		reserved in os_event_wait_low() for the case
		when the caller does not want to pass any
		signal_count value. To distinguish between
		the two cases we initialize signal_count
		to 1 here. */

		signal_count = 1;
	}
}

/** Destructor */
os_event::~os_event() UNIV_NOTHROW
{
#ifdef _WIN32
	if (!srv_use_native_conditions){
		ut_a(CloseHandle(handle));
	} else /*Windows with condition variables */
#endif /* _WIN32 */
	{
		destroy();
		mutex_destroy(&mutex);
	}
}

/**
Creates an event semaphore, i.e., a semaphore which may just have two
states: signaled and nonsignaled. The created event is manual reset: it
must be reset explicitly by calling sync_os_reset_event.
@return	the event handle */
os_event_t
os_event_create(
/*============*/
	const char*	name)			/*!< in: the name of the
						event, if NULL the event
						is created without a name */
{
	return(UT_NEW_NOKEY(os_event(name)));
}

/**
Check if the event is set.
@return true if set */
bool
os_event_is_set(
/*============*/
	const os_event_t	event)		/*!< in: event to test */
{
	return(event->is_set());
}

/**
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
void
os_event_set(
/*=========*/
	os_event_t	event)			/*!< in/out: event to set */
{
	event->set();
}

/**
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low().
@return	current signal_count. */
int64_t
os_event_reset(
/*===========*/
	os_event_t	event)			/*!< in/out: event to reset */
{
	return(event->reset());
}

/**
Waits for an event object until it is in the signaled state or
a timeout is exceeded.
@return	0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
ulint
os_event_wait_time_low(
/*===================*/
	os_event_t	event,			/*!< in/out: event to wait */
	ulint		time_in_usec,		/*!< in: timeout in
						microseconds, or
						OS_SYNC_INFINITE_TIME */
	int64_t		reset_sig_count)	/*!< in: zero or the value
						returned by previous call of
						os_event_reset(). */
{
	return(event->wait_time_low(time_in_usec, reset_sig_count));
}

/**
Waits for an event object until it is in the signaled state.

Where such a scenario is possible, to avoid infinite wait, the
value returned by os_event_reset() should be passed in as
reset_sig_count. */
void
os_event_wait_low(
/*==============*/
	os_event_t	event,			/*!< in: event to wait */
	int64_t		reset_sig_count)	/*!< in: zero or the value
						returned by previous call of
						os_event_reset(). */
{
	event->wait_low(reset_sig_count);
}

/**
Frees an event object. */
void
os_event_destroy(
/*=============*/
	os_event_t&	event)			/*!< in/own: event to free */

{
	if (event != NULL) {
		UT_DELETE(event);
		event = NULL;
	}
}

/**
Initialise the event sub-system. */
void
os_event_init()
{
#ifdef _WIN32
	os_win_init();
#endif /* _WIN32 */
}
