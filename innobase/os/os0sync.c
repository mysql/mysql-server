/******************************************************
The interface to the operating system
synchronization primitives.

(c) 1995 Innobase Oy

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

/* Type definition for an operating system mutex struct */
struct os_mutex_struct{ 
	void*		handle;	/* OS handle to mutex */
	ulint		count;	/* we use this counter to check
				that the same thread does not
				recursively lock the mutex: we
				do not assume that the OS mutex
				supports recursive locking, though
				NT seems to do that */				
};

/*************************************************************
Creates an event semaphore, i.e., a semaphore which may
just have two states: signaled and nonsignaled.
The created event is manual reset: it must be reset
explicitly by calling sync_os_reset_event. */

os_event_t
os_event_create(
/*============*/
			/* out: the event handle */
	char*	name)	/* in: the name of the event, if NULL
			the event is created without a name */
{
#ifdef __WIN__
	HANDLE	event;
	
	event = CreateEvent(NULL,	/* No security attributes */
			TRUE,		/* Manual reset */
			FALSE,		/* Initial state nonsignaled */
			name);
	ut_a(event);

	return(event);
#else
	os_event_t	event;

	UT_NOT_USED(name);

	event = ut_malloc(sizeof(struct os_event_struct));

	os_fast_mutex_init(&(event->os_mutex));
	pthread_cond_init(&(event->cond_var), NULL);

	event->is_set = FALSE;

	return(event);
#endif
}

/*************************************************************
Creates an auto-reset event semaphore, i.e., an event
which is automatically reset when a single thread is
released. */

os_event_t
os_event_create_auto(
/*=================*/
			/* out: the event handle */
	char*	name)	/* in: the name of the event, if NULL
			the event is created without a name */
{
#ifdef __WIN__
	HANDLE	event;

	event = CreateEvent(NULL,	/* No security attributes */
			FALSE,		/* Auto-reset */
			FALSE,		/* Initial state nonsignaled */
			name);
	ut_a(event);

	return(event);
#else
	/* Does nothing in Posix because we do not need this with MySQL  */

	UT_NOT_USED(name);

	return(NULL);
#endif
}

/**************************************************************
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */

void
os_event_set(
/*=========*/
	os_event_t	event)	/* in: event to set */
{
#ifdef __WIN__	
	ut_a(event);
	ut_a(SetEvent(event));
#else
	ut_a(event);

	os_fast_mutex_lock(&(event->os_mutex));

	if (event->is_set) {
		/* Do nothing */
	} else {
		event->is_set = TRUE;
		pthread_cond_broadcast(&(event->cond_var));
	}

	os_fast_mutex_unlock(&(event->os_mutex));
#endif		
}

/**************************************************************
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event. */

void
os_event_reset(
/*===========*/
	os_event_t	event)	/* in: event to reset */
{
#ifdef __WIN__
	ut_a(event);

	ut_a(ResetEvent(event));
#else
	ut_a(event);

	os_fast_mutex_lock(&(event->os_mutex));

	if (!event->is_set) {
		/* Do nothing */
	} else {
		event->is_set = FALSE;
	}

	os_fast_mutex_unlock(&(event->os_mutex));
#endif
}

/**************************************************************
Frees an event object. */

void
os_event_free(
/*==========*/
	os_event_t	event)	/* in: event to free */
	
{
#ifdef __WIN__
	ut_a(event);

	ut_a(CloseHandle(event));
#else
	ut_a(event);

	os_fast_mutex_free(&(event->os_mutex));
	pthread_cond_destroy(&(event->cond_var));

	ut_free(event);
#endif
}

/**************************************************************
Waits for an event object until it is in the signaled state. */

void
os_event_wait(
/*==========*/
	os_event_t	event)	/* in: event to wait */
{
#ifdef __WIN__
	DWORD	err;

	ut_a(event);

	/* Specify an infinite time limit for waiting */
	err = WaitForSingleObject(event, INFINITE);

	ut_a(err == WAIT_OBJECT_0);
#else
	os_fast_mutex_lock(&(event->os_mutex));
loop:
	if (event->is_set == TRUE) {
		os_fast_mutex_unlock(&(event->os_mutex));

		/* Ok, we may return */

		return;
	}

	pthread_cond_wait(&(event->cond_var), &(event->os_mutex));

	/* Solaris manual said that spurious wakeups may occur: we have
	to check the 'is_set' variable again */

	goto loop;
#endif
}

/**************************************************************
Waits for an event object until it is in the signaled state or
a timeout is exceeded. */

ulint
os_event_wait_time(
/*===============*/
				/* out: 0 if success, OS_SYNC_TIME_EXCEEDED if
				timeout was exceeded */
	os_event_t	event,	/* in: event to wait */
	ulint		time)	/* in: timeout in microseconds, or
				OS_SYNC_INFINITE_TIME */
{
#ifdef __WIN__
	DWORD	err;

	ut_a(event);

	if (time != OS_SYNC_INFINITE_TIME) {
		err = WaitForSingleObject(event, time / 1000);
	} else {
		err = WaitForSingleObject(event, INFINITE);
	}
	
	if (err == WAIT_OBJECT_0) {

		return(0);
	} else if (err == WAIT_TIMEOUT) {

		return(OS_SYNC_TIME_EXCEEDED);
	} else {
		ut_error;
		return(1000000); /* dummy value to eliminate compiler warn. */
	}
#else
	UT_NOT_USED(time);
	
	/* In Posix this is just an ordinary, infinite wait */

	os_event_wait(event);

	return(0);
#endif
}

/**************************************************************
Waits for any event in an event array. Returns if even a single
one is signaled or becomes signaled. */

ulint
os_event_wait_multiple(
/*===================*/
					/* out: index of the event
					which was signaled */
	ulint		n,		/* in: number of events in the
					array */
	os_event_t*	event_array)	/* in: pointer to an array of event
					handles */
{
#ifdef __WIN__
	DWORD	index;

	ut_a(event_array);
	ut_a(n > 0);

	index = WaitForMultipleObjects(n,
					event_array,
					FALSE,	   /* Wait for any 1 event */
					INFINITE); /* Infinite wait time
						   limit */
	ut_a(index >= WAIT_OBJECT_0);
	ut_a(index < WAIT_OBJECT_0 + n);

	return(index - WAIT_OBJECT_0);
#else
	ut_a(n == 0);
	
	/* In Posix we can only wait for a single event */

	os_event_wait(*event_array);

	return(0);
#endif
}

/*************************************************************
Creates an operating system mutex semaphore.
Because these are slow, the mutex semaphore of the database
itself (sync_mutex_t) should be used where possible. */

os_mutex_t
os_mutex_create(
/*============*/
			/* out: the mutex handle */
	char*	name)	/* in: the name of the mutex, if NULL
			the mutex is created without a name */
{
#ifdef __WIN__
	HANDLE		mutex;
	os_mutex_t	mutex_str;

	mutex = CreateMutex(NULL,	/* No security attributes */
			FALSE,		/* Initial state: no owner */
			name);
	ut_a(mutex);

	mutex_str = ut_malloc(sizeof(os_mutex_str_t));

	mutex_str->handle = mutex;
	mutex_str->count = 0;

	return(mutex_str);
#else
	os_fast_mutex_t*	os_mutex;
	os_mutex_t		mutex_str;

	UT_NOT_USED(name);
	
	os_mutex = ut_malloc(sizeof(os_fast_mutex_t));

	os_fast_mutex_init(os_mutex);

	mutex_str = ut_malloc(sizeof(os_mutex_str_t));

	mutex_str->handle = os_mutex;
	mutex_str->count = 0;

	return(mutex_str);
#endif	
}

/**************************************************************
Acquires ownership of a mutex semaphore. */

void
os_mutex_enter(
/*===========*/
	os_mutex_t	mutex)	/* in: mutex to acquire */
{
#ifdef __WIN__
	DWORD	err;

	ut_a(mutex);

	/* Specify infinite time limit for waiting */
	err = WaitForSingleObject(mutex->handle, INFINITE);

	ut_a(err == WAIT_OBJECT_0);

	(mutex->count)++;
	ut_a(mutex->count == 1);
#else
	os_fast_mutex_lock(mutex->handle);

	(mutex->count)++;

	ut_a(mutex->count == 1);
#endif
}

/**************************************************************
Releases ownership of a mutex. */

void
os_mutex_exit(
/*==========*/
	os_mutex_t	mutex)	/* in: mutex to release */
{
#ifdef __WIN__
	ut_a(mutex);

	ut_a(mutex->count == 1);

	(mutex->count)--;

	ut_a(ReleaseMutex(mutex->handle));
#else
	ut_a(mutex);

	ut_a(mutex->count == 1);

	(mutex->count)--;

	os_fast_mutex_unlock(mutex->handle);
#endif	
}

/**************************************************************
Frees a mutex object. */

void
os_mutex_free(
/*==========*/
	os_mutex_t	mutex)	/* in: mutex to free */
{
#ifdef __WIN__
	ut_a(mutex);

	ut_a(CloseHandle(mutex->handle));
	ut_free(mutex);
#else
	os_fast_mutex_free(mutex->handle);
	ut_free(mutex->handle);
	ut_free(mutex);
#endif
}

#ifndef _WIN32
/*************************************************************
Initializes an operating system fast mutex semaphore. */

void
os_fast_mutex_init(
/*===============*/
	os_fast_mutex_t*	fast_mutex)	/* in: fast mutex */
{
#ifdef __WIN__
	ut_a(fast_mutex);
	
	InitializeCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	pthread_mutex_init(fast_mutex, NULL);
#endif
}

/**************************************************************
Acquires ownership of a fast mutex. */

void
os_fast_mutex_lock(
/*===============*/
	os_fast_mutex_t*	fast_mutex)	/* in: mutex to acquire */
{
#ifdef __WIN__
	EnterCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	pthread_mutex_lock(fast_mutex);
#endif
}

/**************************************************************
Frees a mutex object. */

void
os_fast_mutex_free(
/*===============*/
	os_fast_mutex_t*	fast_mutex)	/* in: mutex to free */
{
#ifdef __WIN__
	ut_a(fast_mutex);

	DeleteCriticalSection((LPCRITICAL_SECTION) fast_mutex);
#else
	UT_NOT_USED(fast_mutex);

#endif
}
#endif
