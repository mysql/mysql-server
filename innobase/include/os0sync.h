/******************************************************
The interface to the operating system
synchronization primitives.

(c) 1995 Innobase Oy

Created 9/6/1995 Heikki Tuuri
*******************************************************/
#ifndef os0sync_h
#define os0sync_h

#include "univ.i"
#include "ut0lst.h"

#ifdef __WIN__

#define os_fast_mutex_t CRITICAL_SECTION

typedef HANDLE          os_native_event_t;

typedef struct os_event_struct os_event_struct_t;
typedef os_event_struct_t*     os_event_t;

struct os_event_struct {
	os_native_event_t		  handle;
					/* Windows event */
	UT_LIST_NODE_T(os_event_struct_t) os_event_list;
					/* list of all created events */
};
#else
typedef pthread_mutex_t	os_fast_mutex_t;

typedef struct os_event_struct os_event_struct_t;
typedef os_event_struct_t*     os_event_t;

struct os_event_struct {
	os_fast_mutex_t	os_mutex;	/* this mutex protects the next
					fields */
	ibool		is_set;		/* this is TRUE when the event is
					in the signaled state, i.e., a thread
					does not stop if it tries to wait for
					this event */
	ib_longlong	signal_count;	/* this is incremented each time
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

void
os_sync_init(void);
/*==============*/
/*************************************************************
Frees created events and OS 'slow' mutexes. */

void
os_sync_free(void);
/*==============*/
/************************************************************* 
Creates an event semaphore, i.e., a semaphore which may just have two states:
signaled and nonsignaled. The created event is manual reset: it must be reset
explicitly by calling sync_os_reset_event. */

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

void
os_event_set(
/*=========*/
	os_event_t	event);	/* in: event to set */
/**************************************************************
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event. */

void
os_event_reset(
/*===========*/
	os_event_t	event);	/* in: event to reset */
/**************************************************************
Frees an event object. */

void
os_event_free(
/*==========*/
	os_event_t	event);	/* in: event to free */
/**************************************************************
Waits for an event object until it is in the signaled state. If
srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS this also exits the
waiting thread when the event becomes signaled (or immediately if the
event is already in the signaled state). */

void
os_event_wait(
/*==========*/
	os_event_t	event);	/* in: event to wait */
/**************************************************************
Waits for an event object until it is in the signaled state or
a timeout is exceeded. In Unix the timeout is always infinite. */

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

ulint
os_event_wait_multiple(
/*===================*/
					/* out: index of the event
					which was signaled */
	ulint		        n,	/* in: number of events in the
					array */
	os_native_event_t* 	native_event_array);
					/* in: pointer to an array of event
					handles */
#endif
/*************************************************************
Creates an operating system mutex semaphore. Because these are slow, the
mutex semaphore of InnoDB itself (mutex_t) should be used where possible. */

os_mutex_t
os_mutex_create(
/*============*/
				/* out: the mutex handle */
	const char*	name);	/* in: the name of the mutex, if NULL
				the mutex is created without a name */
/**************************************************************
Acquires ownership of a mutex semaphore. */

void
os_mutex_enter(
/*===========*/
	os_mutex_t	mutex);	/* in: mutex to acquire */
/**************************************************************
Releases ownership of a mutex. */

void
os_mutex_exit(
/*==========*/
	os_mutex_t	mutex);	/* in: mutex to release */
/**************************************************************
Frees an mutex object. */

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

void
os_fast_mutex_unlock(
/*=================*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to release */
/*************************************************************
Initializes an operating system fast mutex semaphore. */

void
os_fast_mutex_init(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: fast mutex */
/**************************************************************
Acquires ownership of a fast mutex. */

void
os_fast_mutex_lock(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to acquire */
/**************************************************************
Frees an mutex object. */

void
os_fast_mutex_free(
/*===============*/
	os_fast_mutex_t*	fast_mutex);	/* in: mutex to free */
	
#ifndef UNIV_NONINL
#include "os0sync.ic"
#endif

#endif 
