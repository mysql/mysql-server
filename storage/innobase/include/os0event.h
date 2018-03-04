/*****************************************************************************
Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/os0event.h
The interface to the operating system condition variables

Created 2012-09-23 Sunny Bains (split from os0sync.h)
*******************************************************/

#ifndef os0event_h
#define os0event_h

#include <sys/types.h>

#include "univ.i"

// Forward declaration.
struct os_event;
typedef struct os_event* os_event_t;

/** Denotes an infinite delay for os_event_wait_time() */
#define OS_SYNC_INFINITE_TIME   ULINT_UNDEFINED

/** Return value of os_event_wait_time() when the time is exceeded */
#define OS_SYNC_TIME_EXCEEDED   1

#ifndef UNIV_HOTBACKUP
/**
Creates an event semaphore, i.e., a semaphore which may just have two states:
signaled and nonsignaled. The created event is manual reset: it must be reset
explicitly by calling os_event_reset().
@return	the event handle */
os_event_t
os_event_create(
/*============*/
	const char*	name);	/*!< in: the name of the event, if NULL
				the event is created without a name */

/**
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
void
os_event_set(
/*=========*/
	os_event_t	event);	/*!< in/out: event to set */

/**
Check if the event is set.
@return true if set */
bool
os_event_is_set(
/*============*/
	const os_event_t	event);	/*!< in: event to set */

/**
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low(). */
int64_t
os_event_reset(
/*===========*/
	os_event_t	event);	/*!< in/out: event to reset */

/**
Frees an event object. */
void
os_event_destroy(
/*=============*/
	os_event_t&	event);	/*!< in/own: event to free */

/**
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
void
os_event_wait_low(
/*==============*/
	os_event_t	event,		/*!< in/out: event to wait */
	int64_t		reset_sig_count);/*!< in: zero or the value
					returned by previous call of
					os_event_reset(). */

/** Blocking infinite wait on an event, until signealled.
@param e - event to wait on. */
#define os_event_wait(e) os_event_wait_low((e), 0)

/**
Waits for an event object until it is in the signaled state or
a timeout is exceeded. In Unix the timeout is always infinite.
@return 0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
ulint
os_event_wait_time_low(
/*===================*/
	os_event_t	event,			/*!< in/out: event to wait */
	ulint		time_in_usec,		/*!< in: timeout in
						microseconds, or
						OS_SYNC_INFINITE_TIME */
	int64_t		reset_sig_count);	/*!< in: zero or the value
						returned by previous call of
						os_event_reset(). */

/** Blocking timed wait on an event.
@param e - event to wait on.
@param t - timeout in microseconds */
#define os_event_wait_time(e, t) os_event_wait_time_low((e), (t), 0)

#endif /* !UNIV_HOTBACKUP */
#endif /* !os0event_h */
