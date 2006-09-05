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
#include "srv0start.h"

/* Type definition for an operating system mutex struct */
struct os_mutex_struct{
	void*		handle;	/* OS handle to mutex */
	ulint		count;	/* we use this counter to check
				that the same thread does not
				recursively lock the mutex: we
				do not assume that the OS mutex
				supports recursive locking, though
				NT seems to do that */
	UT_LIST_NODE_T(os_mutex_str_t) os_mutex_list;
				/* list of all 'slow' OS mutexes created */
};

/* Mutex protecting counts and the lists of OS mutexes and events */
os_mutex_t	os_sync_mutex;
ibool		os_sync_mutex_inited	= FALSE;

/* This is incremented by 1 in os_thread_create and decremented by 1 in
os_thread_exit */
ulint	os_thread_count		= 0;

/* The list of all events created */
UT_LIST_BASE_NODE_T(os_event_struct_t)	os_event_list;

/* The list of all OS 'slow' mutexes */
UT_LIST_BASE_NODE_T(os_mutex_str_t)	os_mutex_list;

ulint	os_event_count		= 0;
ulint	os_mutex_count		= 0;
ulint	os_fast_mutex_count	= 0;


/*************************************************************
Initializes global event and OS 'slow' mutex lists. */

void
os_sync_init(void)
/*==============*/
{
	UT_LIST_INIT(os_event_list);
	UT_LIST_INIT(os_mutex_list);

	os_sync_mutex = os_mutex_create(NULL);

	os_sync_mutex_inited = TRUE;
}

/*************************************************************
Frees created events and OS 'slow' mutexes. */

void
os_sync_free(void)
/*==============*/
{
	os_event_t	event;
	os_mutex_t	mutex;

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
}

/*************************************************************
Creates an event semaphore, i.e., a semaphore which may just have two
states: signaled and nonsignaled. The created event is manual reset: it
must be reset explicitly by calling sync_os_reset_event. */

os_event_t
os_event_create(
/*============*/
				/* out: the event handle */
	const char*	name)	/* in: the name of the event, if NULL
				the event is created without a name */
{
#ifdef __WIN__
	os_event_t event;

	event = ut_malloc(sizeof(struct os_event_struct));

	event->handle = CreateEvent(NULL, /* No security attributes */
				    TRUE, /* Manual reset */
				    FALSE, /* Initial state nonsignaled */
				    (LPCTSTR) name);
	if (!event->handle) {
		fprintf(stderr,
			"InnoDB: Could not create a Windows event semaphore;"
			" Windows error %lu\n",
			(ulong) GetLastError());
	}
#else /* Unix */
	os_event_t	event;

	UT_NOT_USED(name);

	event = ut_malloc(sizeof(struct os_event_struct));

	os_fast_mutex_init(&(event->os_mutex));

#if defined(UNIV_HOTBACKUP) && defined(UNIV_HPUX10)
	ut_a(0 == pthread_cond_init(&(event->cond_var),
				    pthread_condattr_default));
#else
	ut_a(0 == pthread_cond_init(&(event->cond_var), NULL));
#endif
	event->is_set = FALSE;
	event->signal_count = 0;
#endif /* __WIN__ */

	/* Put to the list of events */
	os_mutex_enter(os_sync_mutex);

	UT_LIST_ADD_FIRST(os_event_list, os_event_list, event);

	os_event_count++;

	os_mutex_exit(os_sync_mutex);

	return(event);
}

#ifdef __WIN__
/*************************************************************
Creates an auto-reset event semaphore, i.e., an event which is automatically
reset when a single thread is released. Works only in Windows. */

os_event_t
os_event_create_auto(
/*=================*/
				/* out: the event handle */
	const char*	name)	/* in: the name of the event, if NULL
				the event is created without a name */
{
	os_event_t event;

	event = ut_malloc(sizeof(struct os_event_struct));

	event->handle = CreateEvent(NULL, /* No security attributes */
				    FALSE, /* Auto-reset */
				    FALSE, /* Initial state nonsignaled */
				    (LPCTSTR) name);

	if (!event->handle) {
		fprintf(stderr,
			"InnoDB: Could not create a Windows auto"
			" event semaphore; Windows error %lu\n",
			(ulong) GetLastError());
	}

	/* Put to the list of events */
	os_mutex_enter(os_sync_mutex);

	UT_LIST_ADD_FIRST(os_event_list, os_event_list, event);

	os_event_count++;

	os_mutex_exit(os_sync_mutex);

	return(event);
}
#endif

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
	ut_a(SetEvent(event->handle));
#else
	ut_a(event);

	os_fast_mutex_lock(&(event->os_mutex));

	if (event->is_set) {
		/* Do nothing */
	} else {
		event->is_set = TRUE;
		event->signal_count += 1;
		ut_a(0 == pthread_cond_broadcast(&(event->cond_var)));
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

	ut_a(ResetEvent(event->handle));
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

	ut_a(CloseHandle(event->handle));
#else
	ut_a(event);

	os_fast_mutex_free(&(event->os_mutex));
	ut_a(0 == pthread_cond_destroy(&(event->cond_var)));
#endif
	/* Remove from the list of events */

	os_mutex_enter(os_sync_mutex);

	UT_LIST_REMOVE(os_event_list, os_event_list, event);

	os_event_count--;

	os_mutex_exit(os_sync_mutex);

	ut_free(event);
}

/**************************************************************
Waits for an event object until it is in the signaled state. If
srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS this also exits the
waiting thread when the event becomes signaled (or immediately if the
event is already in the signaled state). */

void
os_event_wait(
/*==========*/
	os_event_t	event)	/* in: event to wait */
{
#ifdef __WIN__
	DWORD	err;

	ut_a(event);

	/* Specify an infinite time limit for waiting */
	err = WaitForSingleObject(event->handle, INFINITE);

	ut_a(err == WAIT_OBJECT_0);

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {
		os_thread_exit(NULL);
	}
#else
	ib_longlong	old_signal_count;

	os_fast_mutex_lock(&(event->os_mutex));

	old_signal_count = event->signal_count;

	for (;;) {
		if (event->is_set == TRUE
		    || event->signal_count != old_signal_count) {

			os_fast_mutex_unlock(&(event->os_mutex));

			if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {

				os_thread_exit(NULL);
			}
			/* Ok, we may return */

			return;
		}

		pthread_cond_wait(&(event->cond_var), &(event->os_mutex));

		/* Solaris manual said that spurious wakeups may occur: we
		have to check if the event really has been signaled after
		we came here to wait */
	}
#endif
}

/**************************************************************
Waits for an event object until it is in the signaled state or
a timeout is exceeded. In Unix the timeout is always infinite. */

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
		err = WaitForSingleObject(event->handle, (DWORD) time / 1000);
	} else {
		err = WaitForSingleObject(event->handle, INFINITE);
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

#ifdef __WIN__
/**************************************************************
Waits for any event in an OS native event array. Returns if even a single
one is signaled or becomes signaled. */

ulint
os_event_wait_multiple(
/*===================*/
					/* out: index of the event
					which was signaled */
	ulint			n,	/* in: number of events in the
					array */
	os_native_event_t*	native_event_array)
					/* in: pointer to an array of event
					handles */
{
	DWORD	index;

	ut_a(native_event_array);
	ut_a(n > 0);

	index = WaitForMultipleObjects((DWORD) n, native_event_array,
				       FALSE,	   /* Wait for any 1 event */
				       INFINITE); /* Infinite wait time
						  limit */
	ut_a(index >= WAIT_OBJECT_0);	/* NOTE: Pointless comparision */
	ut_a(index < WAIT_OBJECT_0 + n);

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {
		os_thread_exit(NULL);
	}

	return(index - WAIT_OBJECT_0);
}
#endif

/*************************************************************
Creates an operating system mutex semaphore. Because these are slow, the
mutex semaphore of InnoDB itself (mutex_t) should be used where possible. */

os_mutex_t
os_mutex_create(
/*============*/
				/* out: the mutex handle */
	const char*	name)	/* in: the name of the mutex, if NULL
				the mutex is created without a name */
{
#ifdef __WIN__
	HANDLE		mutex;
	os_mutex_t	mutex_str;

	mutex = CreateMutex(NULL,	/* No security attributes */
			    FALSE,		/* Initial state: no owner */
			    (LPCTSTR) name);
	ut_a(mutex);
#else
	os_fast_mutex_t*	mutex;
	os_mutex_t		mutex_str;

	UT_NOT_USED(name);

	mutex = ut_malloc(sizeof(os_fast_mutex_t));

	os_fast_mutex_init(mutex);
#endif
	mutex_str = ut_malloc(sizeof(os_mutex_str_t));

	mutex_str->handle = mutex;
	mutex_str->count = 0;

	if (os_sync_mutex_inited) {
		/* When creating os_sync_mutex itself we cannot reserve it */
		os_mutex_enter(os_sync_mutex);
	}

	UT_LIST_ADD_FIRST(os_mutex_list, os_mutex_list, mutex_str);

	os_mutex_count++;

	if (os_sync_mutex_inited) {
		os_mutex_exit(os_sync_mutex);
	}

	return(mutex_str);
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
	ut_a(mutex);

	ut_a(mutex->count == 1);

	(mutex->count)--;
#ifdef __WIN__
	ut_a(ReleaseMutex(mutex->handle));
#else
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
	ut_a(mutex);

	if (os_sync_mutex_inited) {
		os_mutex_enter(os_sync_mutex);
	}

	UT_LIST_REMOVE(os_mutex_list, os_mutex_list, mutex);

	os_mutex_count--;

	if (os_sync_mutex_inited) {
		os_mutex_exit(os_sync_mutex);
	}

#ifdef __WIN__
	ut_a(CloseHandle(mutex->handle));

	ut_free(mutex);
#else
	os_fast_mutex_free(mutex->handle);
	ut_free(mutex->handle);
	ut_free(mutex);
#endif
}

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
#if defined(UNIV_HOTBACKUP) && defined(UNIV_HPUX10)
	ut_a(0 == pthread_mutex_init(fast_mutex, pthread_mutexattr_default));
#else
	ut_a(0 == pthread_mutex_init(fast_mutex, MY_MUTEX_INIT_FAST));
#endif
#endif
	if (os_sync_mutex_inited) {
		/* When creating os_sync_mutex itself (in Unix) we cannot
		reserve it */

		os_mutex_enter(os_sync_mutex);
	}

	os_fast_mutex_count++;

	if (os_sync_mutex_inited) {
		os_mutex_exit(os_sync_mutex);
	}
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
Releases ownership of a fast mutex. */

void
os_fast_mutex_unlock(
/*=================*/
	os_fast_mutex_t*	fast_mutex)	/* in: mutex to release */
{
#ifdef __WIN__
	LeaveCriticalSection(fast_mutex);
#else
	pthread_mutex_unlock(fast_mutex);
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
	int	ret;

	ret = pthread_mutex_destroy(fast_mutex);

	if (ret != 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: error: return value %lu when calling\n"
			"InnoDB: pthread_mutex_destroy().\n", (ulint)ret);
		fprintf(stderr,
			"InnoDB: Byte contents of the pthread mutex at %p:\n",
			(void*) fast_mutex);
		ut_print_buf(stderr, fast_mutex, sizeof(os_fast_mutex_t));
		fprintf(stderr, "\n");
	}
#endif
	if (os_sync_mutex_inited) {
		/* When freeing the last mutexes, we have
		already freed os_sync_mutex */

		os_mutex_enter(os_sync_mutex);
	}

	os_fast_mutex_count--;

	if (os_sync_mutex_inited) {
		os_mutex_exit(os_sync_mutex);
	}
}
