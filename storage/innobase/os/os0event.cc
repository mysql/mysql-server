/*****************************************************************************

Copyright (c) 2013, 2019, Oracle and/or its affiliates. All Rights Reserved.

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
static const uint64_t MICROSECS_IN_A_SECOND = 1000000;

/** The number of nanoseconds in a second. */
static const uint64_t NANOSECS_IN_A_SECOND = 1000 * MICROSECS_IN_A_SECOND;

#ifdef _WIN32
/** Native condition variable. */
typedef CONDITION_VARIABLE	os_cond_t;
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

	friend void os_event_global_init();
	friend void os_event_global_destroy();

	/**
	Destroys a condition variable */
	void destroy() UNIV_NOTHROW
	{
#ifndef _WIN32
		int	ret = pthread_cond_destroy(&cond_var);
		ut_a(ret == 0);
#endif /* !_WIN32 */

		mutex.destroy();
	}

	/** Set the event */
	void set() UNIV_NOTHROW
	{
		mutex.enter();

		if (!m_set) {
			broadcast();
		}

		mutex.exit();
	}

	int64_t reset() UNIV_NOTHROW
	{
		mutex.enter();

		if (m_set) {
			m_set = false;
		}

		int64_t	ret = signal_count;

		mutex.exit();

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

		mutex.init();

#ifdef _WIN32
		InitializeConditionVariable(&cond_var);
#else
		{
			int	ret;

			ret = pthread_cond_init(&cond_var, &cond_attr);
			ut_a(ret == 0);
		}
#endif /* _WIN32 */
	}

	/**
	Wait on condition variable */
	void wait() UNIV_NOTHROW
	{
#ifdef _WIN32
		if (!SleepConditionVariableCS(&cond_var, mutex, INFINITE)) {
			ut_error;
		}
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
		WakeAllConditionVariable(&cond_var);
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
		WakeConditionVariable(&cond_var);
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
#ifndef _WIN32
	/** Returns absolute time until which we should wait if
	we wanted to wait for time_in_usec microseconds since now. */
	struct timespec get_wait_timelimit(ulint time_in_usec);
#endif /* !_WIN32 */

private:
	bool			m_set;		/*!< this is true when the
						event is in the signaled
						state, i.e., a thread does
						not stop if it tries to wait
						for this event */
	int64_t			signal_count;	/*!< this is incremented
						each time the event becomes
						signaled */
	EventMutex		mutex;		/*!< this mutex protects
						the next fields */


	os_cond_t		cond_var;	/*!< condition variable is
						used in waiting for the event */
#ifndef _WIN32
	/** Attributes object passed to pthread_cond_* functions.
	Defines usage of the monotonic clock if it's available.
	Initialized once, in the os_event::global_init(), and
	destroyed in the os_event::global_destroy(). */
	static pthread_condattr_t cond_attr;

	/** True iff usage of the monotonic clock has been successfuly
	enabled for the cond_attr object. */
	static bool cond_attr_has_monotonic_clock;
#endif /* !_WIN32 */
	static bool global_initialized;

public:
	event_iter_t		event_iter;	/*!< For O(1) removal from
						list */
protected:
	// Disable copying
	os_event(const os_event&);
	os_event& operator=(const os_event&);
};

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
	BOOL		ret;

	ret = SleepConditionVariableCS(&cond_var, mutex, time_in_ms);

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
	mutex.enter();

	if (!reset_sig_count) {
		reset_sig_count = signal_count;
	}

	while (!m_set && signal_count == reset_sig_count) {

		wait();

		/* Spurious wakeups may occur: we have to check if the
		event really has been signaled after we came here to wait. */
	}

	mutex.exit();
}

#ifndef _WIN32

struct timespec os_event::get_wait_timelimit(ulint time_in_usec) {
    for (int i = 0;; i++) {
	ut_a(i < 10);
	if (cond_attr_has_monotonic_clock) {
	    struct timespec tp;
	    if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
		int errno_clock_gettime = errno;

#ifndef UNIV_NO_ERR_MSGS
		ib::error() << "clock_gettime() failed: " <<
			strerror(errno_clock_gettime);
#endif /* !UNIV_NO_ERR_MSGS */

		os_thread_sleep(100000); /* 0.1 sec */
		errno = errno_clock_gettime;

	    } else {
		const uint64_t increased = tp.tv_nsec +
				       time_in_usec * 1000;
		if (increased >= NANOSECS_IN_A_SECOND) {
		  tp.tv_sec += increased / NANOSECS_IN_A_SECOND;
		  tp.tv_nsec = increased % NANOSECS_IN_A_SECOND;
		} else {
		  tp.tv_nsec = increased;
		}
		return (tp);
	      }

	} else {
	    struct timeval tv;
	    if (gettimeofday(&tv, NULL) == -1) {
		const int errno_gettimeofday = errno;

#ifndef UNIV_NO_ERR_MSGS
		ib::error( ) << "clock_gettime() failed: " <<
			strerror(errno_gettimeofday);
#endif /* !UNIV_NO_ERR_MSGS */

		os_thread_sleep(100000); /* 0.1 sec */
		errno = errno_gettimeofday;

	    } else {
		uint64_t increased = tv.tv_usec + uint64_t(time_in_usec);

		if (increased >= MICROSECS_IN_A_SECOND) {
		    tv.tv_sec += increased / MICROSECS_IN_A_SECOND;
		    tv.tv_usec = increased % MICROSECS_IN_A_SECOND;
		} else {
		    tv.tv_usec = increased;
		}

		struct timespec abstime;
		abstime.tv_sec = tv.tv_sec;
		abstime.tv_nsec = tv.tv_usec * 1000;
		return (abstime);
	    }
	}
    }
}

#endif /* !_WIN32 */

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

	if (time_in_usec != OS_SYNC_INFINITE_TIME) {
		time_in_ms = DWORD(time_in_usec / 1000);
	} else {
		time_in_ms = INFINITE;
	}
#else
	struct timespec	abstime;

	if (time_in_usec != OS_SYNC_INFINITE_TIME) {
		abstime = os_event::get_wait_timelimit(time_in_usec);
	} else {
		abstime.tv_nsec = 999999999;
		abstime.tv_sec = (time_t) ULINT_MAX;
	}

	ut_a(abstime.tv_nsec <= 999999999);

#endif /* _WIN32 */

	mutex.enter();

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

	mutex.exit();

	return(timed_out ? OS_SYNC_TIME_EXCEEDED : 0);
}

/** Constructor */
os_event::os_event(const char* name) UNIV_NOTHROW
{
	ut_a(global_initialized);
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

/** Destructor */
os_event::~os_event() UNIV_NOTHROW
{
	destroy();
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

#ifndef _WIN32
pthread_condattr_t os_event::cond_attr;
bool os_event::cond_attr_has_monotonic_clock (false);
#endif /* !_WIN32 */
bool os_event::global_initialized (false);


void os_event_global_init(void) {
#ifndef _WIN32
	int ret = pthread_condattr_init(&os_event::cond_attr);
	ut_a(ret == 0);

#ifdef UNIV_LINUX /* MacOS does not have support. */
#ifdef HAVE_CLOCK_GETTIME
	ret = pthread_condattr_setclock(&os_event::cond_attr, CLOCK_MONOTONIC);
	if (ret == 0) {
	  os_event::cond_attr_has_monotonic_clock = true;
  }
#endif /* HAVE_CLOCK_GETTIME */

#ifndef UNIV_NO_ERR_MSGS
  if (!os_event::cond_attr_has_monotonic_clock) {
    ib::warn() << "CLOCK_MONOTONIC is unsupported, so do not change the" <<
	          " system time when MySQL is running !";
  }
#endif /* !UNIV_NO_ERR_MSGS */

#endif /* UNIV_LINUX */
#endif /* !_WIN32 */
	os_event::global_initialized = true;
}

void os_event_global_destroy(void) {
	ut_a(os_event::global_initialized);
#ifndef _WIN32
	os_event::cond_attr_has_monotonic_clock = false;
#ifdef UNIV_DEBUG
	const int ret =
#endif /* UNIV_DEBUG */
		pthread_condattr_destroy(&os_event::cond_attr);
	ut_ad(ret == 0);
#endif /* !_WIN32 */
	os_event::global_initialized = false;
}
