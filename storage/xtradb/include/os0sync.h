/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
The interface to the operating system
synchronization primitives.

Created 9/6/1995 Heikki Tuuri
*******************************************************/

#ifndef os0sync_h
#define os0sync_h

#include "univ.i"
#include "ut0lst.h"

#ifdef __WIN__

#define os_fast_mutex_t CRITICAL_SECTION

typedef HANDLE		os_native_event_t;

typedef struct os_event_struct	os_event_struct_t;
typedef os_event_struct_t*	os_event_t;

struct os_event_struct {
	os_native_event_t		  handle;
					/* Windows event */
	UT_LIST_NODE_T(os_event_struct_t) os_event_list;
					/* list of all created events */
};
#else
typedef pthread_mutex_t	os_fast_mutex_t;

typedef struct os_event_struct	os_event_struct_t;
typedef os_event_struct_t*	os_event_t;

struct os_event_struct {
	os_fast_mutex_t	os_mutex;	/* this mutex protects the next
					fields */
	ibool		is_set;		/* this is TRUE when the event is
					in the signaled state, i.e., a thread
					does not stop if it tries to wait for
					this event */
	ib_int64_t	signal_count;	/* this is incremented each time
					the event becomes signaled */
	pthread_cond_t	cond_var;	/* condition variable is used in
					waiting for the event */
	UT_LIST_NODE_T(os_event_struct_t) os_event_list;
					/* list of all created events */
};
#endif

typedef struct os_mutex_struct	os_mutex_str_t;
typedef os_mutex_str_t*		os_mutex_t;

#define OS_SYNC_INFINITE_TIME	((ulint)(-1))

#define OS_SYNC_TIME_EXCEEDED	1

/* Mutex protecting counts and the event and OS 'slow' mutex lists */
extern os_mutex_t	os_sync_mutex;

/* This is incremented by 1 in os_thread_create and decremented by 1 in
os_thread_exit */
extern ulint		os_thread_count;

extern ulint		os_event_count;
extern ulint		os_mutex_count;
extern ulint		os_fast_mutex_count;

/*************************************************************
Initializes global event and OS 'slow' mutex lists. */
UNIV_INTERN
void
os_sync_init(void);
/*==============*/
/*************************************************************
Frees created events and OS 'slow' mutexes. */
UNIV_INTERN
void
os_sync_free(void);
/*==============*/
/*************************************************************
Creates an event semaphore, i.e., a semaphore which may just have two states:
signaled and nonsignaled. The created event is manual reset: it must be reset
explicitly by calling sync_os_reset_event. */
UNIV_INTERN
os_event_t
os_event_create(
/*============*/
				/* out: the event handle */
	const char*	name);	/* in: the name of the event, if NULL
				the event is created without a name */
#ifdef __WIN__
/*************************************************************
Creates an auto-reset event semaphore, i.e., an event which is automatically
reset when a single thread is released. Works only in Windows. */
UNIV_INTERN
os_event_t
os_event_create_auto(
/*=================*/
				/* out: the event handle */
	const char*	name);	/* in: the name of the event, if NULL
				the event is created without a name */
#endif
/**************************************************************
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
UNIV_INTERN
void
os_event_set(
/*=========*/
	os_event_t	event);	/* in: event to set */
/**************************************************************
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low(). */
UNIV_INTERN
ib_int64_t
os_event_reset(
/*===========*/
	os_event_t	event);	/* in: event to reset */
/**************************************************************
Frees an event object. */
UNIV_INTERN
void
os_event_free(
/*==========*/
	os_event_t	event);	/* in: event to free */

/**************************************************************
Waits for an event object until it is in the signaled state. If
srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS this also exits the
waiting thread when the event becomes signaled (or immediately if the
event is already in the signaled state).

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
	os_event_t	event,		/* in: event to wait */
	ib_int64_t	reset_sig_count);/* in: zero or the value
					returned by previous call of
					os_event_reset(). */

#define os_event_wait(event) os_event_wait_low(event, 0)

/**************************************************************
Waits for an event object until it is in the signaled state or
a timeout is exceeded. In Unix the timeout is always infinite. */
UNIV_INTERN
ulint
os_event_wait_time(
/*===============*/
				/* out: 0 if success,
				OS_SYNC_TIME_EXCEEDED if timeout
				was exceeded */
	os_event_t	event,	/* in: event to wait */
	ulint		time);	/* in: timeout in microseconds, or
				OS_SYNC_INFINITE_TIME */
#ifdef __WIN__
/**************************************************************
Waits for any event in an OS native event array. Returns if even a single
one is signaled or becomes signaled. */
UNIV_INTERN
ulint
os_event_wait_multiple(
/*===================*/
					/* out: index of the event
					which was signaled */
	ulint			n,	/* in: number of events in the
					array */
	os_native_event_t*	native_event_array);
					/* in: pointer to an array of event
					handles */
#endif
/*************************************************************
Creates an operating system mutex semaphore. Because these are slow, the
mutex semaphore of InnoDB itself (mutex_t) should be used where possible. */
UNIV_INTERN
os_mutex_t
os_mutex_create(
/*============*/
				/* out: the mutex handle */
	const char*	name);	/* in: the name of the mutex, if NULL
				the mutex is created without a name */
/**************************************************************
Acquires ownership of a mutex semaphore. */
UNIV_INTERN
void
os_mutex_enter(
/*===========*/
	os_mutex_t	mutex);	/* in: mutex to acquire */
/**************************************************************
Releases ownership of a mutex. */
UNIV_INTERN
void
os_mutex_exit(
/*==========*/
	os_mutex_t	mutex);	/* in: mutex to release */
/**************************************************************
Frees an mutex object. */
UNIV_INTERN
void
os_mutex_free(
/*==========*/
	os_mutex_t	mutex);	/* in: mutex to free */
/**************************************************************
Acquires ownership of a fast mutex. Currently in Windows this is the same
as os_fast_mutex_lock! */
UNIV_INLINE
ulint
os_fast_mutex_trylock(
/*==================*/
						/* out: 0 if success, != 0 if
						was reserved by another
						thread */
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to acquire */
/**************************************************************
Releases ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_unlock(
/*=================*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to release */
/*************************************************************
Initializes an operating system fast mutex semaphore. */
UNIV_INTERN
void
os_fast_mutex_init(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: fast mutex */
/**************************************************************
Acquires ownership of a fast mutex. */
UNIV_INTERN
void
os_fast_mutex_lock(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to acquire */
/**************************************************************
Frees an mutex object. */
UNIV_INTERN
void
os_fast_mutex_free(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to free */

#ifdef HAVE_GCC_ATOMIC_BUILTINS
/**************************************************************
Atomic compare-and-swap for InnoDB. Currently requires GCC atomic builtins.
Returns true if swapped, ptr is pointer to target, old_val is value to
compare to, new_val is the value to swap in. */
#define os_compare_and_swap(ptr, old_val, new_val) \
	__sync_bool_compare_and_swap(ptr, old_val, new_val)

/**************************************************************
Atomic increment for InnoDB. Currently requires GCC atomic builtins.
Returns the resulting value, ptr is pointer to target, amount is the
amount of increment. */
#define os_atomic_increment(ptr, amount) \
	__sync_add_and_fetch(ptr, amount)

#endif /* HAVE_GCC_ATOMIC_BUILTINS */

#ifndef UNIV_NONINL
#include "os0sync.ic"
#endif

#endif
