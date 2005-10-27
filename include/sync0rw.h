/******************************************************
The read-write lock (for threads, not for database transactions)

(c) 1995 Innobase Oy

Created 9/11/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0rw_h
#define sync0rw_h

#include "univ.i"
#include "ut0lst.h"
#include "sync0sync.h"
#include "os0sync.h"

/* The following undef is to prevent a name conflict with a macro
in MySQL: */
#undef rw_lock_t

/* Latch types; these are used also in btr0btr.h: keep the numerical values
smaller than 30 and the order of the numerical values like below! */
#define RW_S_LATCH	1
#define	RW_X_LATCH	2
#define	RW_NO_LATCH	3

typedef struct rw_lock_struct		rw_lock_t;
#ifdef UNIV_SYNC_DEBUG
typedef struct rw_lock_debug_struct	rw_lock_debug_t;
#endif /* UNIV_SYNC_DEBUG */

typedef UT_LIST_BASE_NODE_T(rw_lock_t)	rw_lock_list_t;

extern rw_lock_list_t 	rw_lock_list;
extern mutex_t		rw_lock_list_mutex;

#ifdef UNIV_SYNC_DEBUG
/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be

acquired in addition to the mutex protecting the lock. */
extern mutex_t		rw_lock_debug_mutex;
extern os_event_t	rw_lock_debug_event;	/* If deadlock detection does
					not get immediately the mutex it
					may wait for this event */
extern ibool		rw_lock_debug_waiters;	/* This is set to TRUE, if
					there may be waiters for the event */
#endif /* UNIV_SYNC_DEBUG */

extern	ulint	rw_s_system_call_count;
extern	ulint	rw_s_spin_wait_count;
extern	ulint	rw_s_exit_count;
extern	ulint	rw_s_os_wait_count;
extern	ulint	rw_x_system_call_count;
extern	ulint	rw_x_spin_wait_count;
extern	ulint	rw_x_os_wait_count;
extern	ulint	rw_x_exit_count;

/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
#define rw_lock_create(L) rw_lock_create_func((L), __FILE__, __LINE__, #L)
          
/*=====================*/
/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */

void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/* in: pointer to memory */
	const char*	cfile_name,	/* in: file name where created */
  ulint cline,  /* in: file line where created */
  const char* cmutex_name); /* in: mutex name */
/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */

void
rw_lock_free(
/*=========*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks. */

ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock);
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */

#define rw_lock_s_lock(M)    rw_lock_s_lock_func(\
					  (M), 0, __FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */

#define rw_lock_s_lock_gen(M, P)    rw_lock_s_lock_func(\
					  (M), (P), __FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw s-locking, not the
corresponding function. */

#define rw_lock_s_lock_nowait(M)    rw_lock_s_lock_func_nowait(\
					     (M), __FILE__, __LINE__)
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode
for the current thread. If the rw-lock is locked in exclusive mode, or
there is an exclusive lock request waiting, the function spins a preset
time (controlled by SYNC_SPIN_ROUNDS), waiting for the lock, before
suspending the thread. */
UNIV_INLINE
void
rw_lock_s_lock_func(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode
for the current thread if the lock can be acquired immediately. */
UNIV_INLINE
ibool
rw_lock_s_lock_func_nowait(
/*=======================*/
				/* out: TRUE if success */
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread if the lock can be
obtained immediately. */
UNIV_INLINE
ibool
rw_lock_x_lock_func_nowait(
/*=======================*/
				/* out: TRUE if success */
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
Releases a shared mode lock. */
UNIV_INLINE
void
rw_lock_s_unlock_func(
/*==================*/
	rw_lock_t*	lock	/* in: rw-lock */
#ifdef UNIV_SYNC_DEBUG
	,ulint		pass	/* in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	);
/***********************************************************************
Releases a shared mode lock. */

#ifdef UNIV_SYNC_DEBUG
#define rw_lock_s_unlock(L)    rw_lock_s_unlock_func(L, 0)
#else
#define rw_lock_s_unlock(L)    rw_lock_s_unlock_func(L)
#endif
/***********************************************************************
Releases a shared mode lock. */

#ifdef UNIV_SYNC_DEBUG
#define rw_lock_s_unlock_gen(L, P)    rw_lock_s_unlock_func(L, P)
#else
#define rw_lock_s_unlock_gen(L, P)    rw_lock_s_unlock_func(L)
#endif
/******************************************************************
NOTE! The following macro should be used in rw x-locking, not the
corresponding function. */

#define rw_lock_x_lock(M)    rw_lock_x_lock_func(\
					  (M), 0, __FILE__, __LINE__)
/******************************************************************
NOTE! The following macro should be used in rw x-locking, not the
corresponding function. */

#define rw_lock_x_lock_gen(M, P)    rw_lock_x_lock_func(\
					  (M), (P), __FILE__, __LINE__)
/******************************************************************
NOTE! The following macros should be used in rw x-locking, not the
corresponding function. */

#define rw_lock_x_lock_nowait(M)    rw_lock_x_lock_func_nowait(\
					     (M), __FILE__, __LINE__)
/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */

void
rw_lock_x_lock_func(
/*================*/
        rw_lock_t*   	lock,  	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line);	/* in: line where requested */
/**********************************************************************
Releases an exclusive mode lock. */
UNIV_INLINE
void
rw_lock_x_unlock_func(
/*==================*/
	rw_lock_t*	lock	/* in: rw-lock */
#ifdef UNIV_SYNC_DEBUG
	,ulint		pass	/* in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	);
/***********************************************************************
Releases an exclusive mode lock. */

#ifdef UNIV_SYNC_DEBUG
#define rw_lock_x_unlock(L)    rw_lock_x_unlock_func(L, 0)
#else
#define rw_lock_x_unlock(L)    rw_lock_x_unlock_func(L)
#endif
/***********************************************************************
Releases an exclusive mode lock. */

#ifdef UNIV_SYNC_DEBUG
#define rw_lock_x_unlock_gen(L, P)    rw_lock_x_unlock_func(L, P)
#else
#define rw_lock_x_unlock_gen(L, P)    rw_lock_x_unlock_func(L)
#endif
/**********************************************************************
Low-level function which locks an rw-lock in s-mode when we know that it
is possible and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
UNIV_INLINE
void
rw_lock_s_lock_direct(
/*==================*/
	rw_lock_t*	lock,		/* in: pointer to rw-lock */
	const char*	file_name,	/* in: file name where requested */
	ulint		line		/* in: line where lock requested */
);
/**********************************************************************
Low-level function which locks an rw-lock in x-mode when we know that it
is not locked and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
UNIV_INLINE
void
rw_lock_x_lock_direct(
/*==================*/
	rw_lock_t*	lock,		/* in: pointer to rw-lock */
	const char*	file_name,	/* in: file name where requested */
	ulint		line		/* in: line where lock requested */
);
/**********************************************************************
This function is used in the insert buffer to move the ownership of an
x-latch on a buffer frame to the current thread. The x-latch was set by
the buffer read operation and it protected the buffer frame while the
read was done. The ownership is moved because we want that the current
thread is able to acquire a second x-latch which is stored in an mtr.
This, in turn, is needed to pass the debug checks of index page
operations. */

void
rw_lock_x_lock_move_ownership(
/*==========================*/
	rw_lock_t*	lock);	/* in: lock which was x-locked in the
				buffer read */
/**********************************************************************
Releases a shared mode lock when we know there are no waiters and none
else will access the lock during the time this function is executed. */
UNIV_INLINE
void
rw_lock_s_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Releases an exclusive mode lock when we know there are no waiters, and
none else will access the lock durint the time this function is executed. */
UNIV_INLINE
void
rw_lock_x_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/* in: rw-lock */
/**********************************************************************
Sets the rw-lock latching level field. */

void
rw_lock_set_level(
/*==============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	ulint		level);	/* in: level */
/**********************************************************************
Returns the value of writer_count for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call. */
UNIV_INLINE
ulint
rw_lock_get_x_lock_count(
/*=====================*/
				/* out: value of writer_count */
	rw_lock_t*	lock);	/* in: rw-lock */
/************************************************************************
Accessor functions for rw lock. */
UNIV_INLINE
ulint
rw_lock_get_waiters(
/*================*/
	rw_lock_t*	lock);
UNIV_INLINE
ulint
rw_lock_get_writer(
/*===============*/
	rw_lock_t*	lock);
UNIV_INLINE
ulint
rw_lock_get_reader_count(
/*=====================*/
	rw_lock_t*	lock);
#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */

ibool
rw_lock_own(
/*========*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type);	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
#endif /* UNIV_SYNC_DEBUG */
/**********************************************************************
Checks if somebody has locked the rw-lock in the specified mode. */

ibool
rw_lock_is_locked(
/*==============*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type);	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
#ifdef UNIV_SYNC_DEBUG
/*******************************************************************
Prints debug info of an rw-lock. */

void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock);	/* in: rw-lock */
/*******************************************************************
Prints debug info of currently locked rw-locks. */

void
rw_lock_list_print_info(void);
/*=========================*/
/*******************************************************************
Returns the number of currently locked rw-locks.
Works only in the debug version. */

ulint
rw_lock_n_locked(void);
/*==================*/

/*#####################################################################*/

/**********************************************************************
Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */

void
rw_lock_debug_mutex_enter(void);
/*==========================*/
/**********************************************************************
Releases the debug mutex. */

void
rw_lock_debug_mutex_exit(void);
/*==========================*/
/*************************************************************************
Prints info of a debug struct. */

void
rw_lock_debug_print(
/*================*/
	rw_lock_debug_t*	info);	/* in: debug struct */
#endif /* UNIV_SYNC_DEBUG */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! The structure used in the spin lock
implementation of a read-write lock. Several threads may have a shared lock
simultaneously in this lock, but only one writer may have an exclusive lock,
in which case no shared locks are allowed. To prevent starving of a writer
blocked by readers, a writer may queue for the lock by setting the writer
field. Then no new readers are allowed in. */

struct rw_lock_struct {
	ulint	reader_count;	/* Number of readers who have locked this
				lock in the shared mode */
	ulint	writer; 	/* This field is set to RW_LOCK_EX if there
				is a writer owning the lock (in exclusive
				mode), RW_LOCK_WAIT_EX if a writer is
				queueing for the lock, and
				RW_LOCK_NOT_LOCKED, otherwise. */
	os_thread_id_t	writer_thread;
				/* Thread id of a possible writer thread */
	ulint	writer_count;	/* Number of times the same thread has
				recursively locked the lock in the exclusive
				mode */
	mutex_t	mutex;		/* The mutex protecting rw_lock_struct */
	ulint	pass; 		/* Default value 0. This is set to some
				value != 0 given by the caller of an x-lock
				operation, if the x-lock is to be passed to
				another thread to unlock (which happens in
				asynchronous i/o). */
	ulint	waiters;	/* This ulint is set to 1 if there are
				waiters (readers or writers) in the global
				wait array, waiting for this rw_lock.
				Otherwise, == 0. */
	ibool	writer_is_wait_ex;
				/* This is TRUE if the writer field is
				RW_LOCK_WAIT_EX; this field is located far
				from the memory update hotspot fields which
				are at the start of this struct, thus we can
				peek this field without causing much memory
				bus traffic */
	UT_LIST_NODE_T(rw_lock_t) list;
				/* All allocated rw locks are put into a
				list */
#ifdef UNIV_SYNC_DEBUG
	UT_LIST_BASE_NODE_T(rw_lock_debug_t) debug_list;
				/* In the debug version: pointer to the debug
				info list of the lock */
#endif /* UNIV_SYNC_DEBUG */
	ulint	level;		/* Level in the global latching
				order; default SYNC_LEVEL_NONE */
	const char*	cfile_name;/* File name where lock created */
	ulint	cline;		/* Line where created */
	const char*	last_s_file_name;/* File name where last s-locked */
	const char*	last_x_file_name;/* File name where last x-locked */
	ulint	last_s_line;	/* Line number where last time s-locked */
	ulint	last_x_line;	/* Line number where last time x-locked */
	ulint	magic_n;
};

#define	RW_LOCK_MAGIC_N	22643

#ifdef UNIV_SYNC_DEBUG
/* The structure for storing debug info of an rw-lock */
struct	rw_lock_debug_struct {

	os_thread_id_t thread_id;  /* The thread id of the thread which
				locked the rw-lock */
	ulint	pass;		/* Pass value given in the lock operation */
	ulint	lock_type;	/* Type of the lock: RW_LOCK_EX,
				RW_LOCK_SHARED, RW_LOCK_WAIT_EX */
	const char*	file_name;/* File name where the lock was obtained */
	ulint	line;		/* Line where the rw-lock was locked */
	UT_LIST_NODE_T(rw_lock_debug_t) list;
				/* Debug structs are linked in a two-way
				list */
};
#endif /* UNIV_SYNC_DEBUG */

#ifndef UNIV_NONINL
#include "sync0rw.ic"
#endif

#endif
