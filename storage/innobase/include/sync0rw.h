/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.
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

/**************************************************//**
@file include/sync0rw.h
The read-write lock (for threads, not for database transactions)

Created 9/11/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0rw_h
#define sync0rw_h

#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "ut0lst.h"
#include "sync0sync.h"
#include "os0sync.h"

/* The following undef is to prevent a name conflict with a macro
in MySQL: */
#undef rw_lock_t
#endif /* !UNIV_HOTBACKUP */

/* Latch types; these are used also in btr0btr.h: keep the numerical values
smaller than 30 and the order of the numerical values like below! */
#define RW_S_LATCH	1
#define	RW_X_LATCH	2
#define	RW_NO_LATCH	3

#ifndef UNIV_HOTBACKUP
/* We decrement lock_word by this amount for each x_lock. It is also the
start value for the lock_word, meaning that it limits the maximum number
of concurrent read locks before the rw_lock breaks. The current value of
0x00100000 allows 1,048,575 concurrent readers and 2047 recursive writers.*/
#define X_LOCK_DECR		0x00100000

typedef struct rw_lock_struct		rw_lock_t;
#ifdef UNIV_SYNC_DEBUG
typedef struct rw_lock_debug_struct	rw_lock_debug_t;
#endif /* UNIV_SYNC_DEBUG */

typedef UT_LIST_BASE_NODE_T(rw_lock_t)	rw_lock_list_t;

extern rw_lock_list_t	rw_lock_list;
extern mutex_t		rw_lock_list_mutex;

#ifdef UNIV_SYNC_DEBUG
/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be

acquired in addition to the mutex protecting the lock. */
extern mutex_t		rw_lock_debug_mutex;
extern os_event_t	rw_lock_debug_event;	/*!< If deadlock detection does
					not get immediately the mutex it
					may wait for this event */
extern ibool		rw_lock_debug_waiters;	/*!< This is set to TRUE, if
					there may be waiters for the event */
#endif /* UNIV_SYNC_DEBUG */

/** number of spin waits on rw-latches,
resulted during exclusive (write) locks */
extern	ib_int64_t	rw_s_spin_wait_count;
/** number of spin loop rounds on rw-latches,
resulted during exclusive (write) locks */
extern	ib_int64_t	rw_s_spin_round_count;
/** number of unlocks (that unlock shared locks),
set only when UNIV_SYNC_PERF_STAT is defined */
extern	ib_int64_t	rw_s_exit_count;
/** number of OS waits on rw-latches,
resulted during shared (read) locks */
extern	ib_int64_t	rw_s_os_wait_count;
/** number of spin waits on rw-latches,
resulted during shared (read) locks */
extern	ib_int64_t	rw_x_spin_wait_count;
/** number of spin loop rounds on rw-latches,
resulted during shared (read) locks */
extern	ib_int64_t	rw_x_spin_round_count;
/** number of OS waits on rw-latches,
resulted during exclusive (write) locks */
extern	ib_int64_t	rw_x_os_wait_count;
/** number of unlocks (that unlock exclusive locks),
set only when UNIV_SYNC_PERF_STAT is defined */
extern	ib_int64_t	rw_x_exit_count;

#ifdef UNIV_PFS_RWLOCK
/* Following are rwlock keys used to register with MySQL
performance schema */
# ifdef UNIV_LOG_ARCHIVE
extern	mysql_pfs_key_t	archive_lock_key;
# endif /* UNIV_LOG_ARCHIVE */
extern	mysql_pfs_key_t btr_search_latch_key;
extern	mysql_pfs_key_t	buf_block_lock_key;
# ifdef UNIV_SYNC_DEBUG
extern	mysql_pfs_key_t	buf_block_debug_latch_key;
# endif /* UNIV_SYNC_DEBUG */
extern	mysql_pfs_key_t	dict_operation_lock_key;
extern	mysql_pfs_key_t	fil_space_latch_key;
extern	mysql_pfs_key_t	checkpoint_lock_key;
extern	mysql_pfs_key_t	trx_i_s_cache_lock_key;
extern	mysql_pfs_key_t	trx_purge_latch_key;
extern	mysql_pfs_key_t	index_tree_rw_lock_key;
extern	mysql_pfs_key_t	dict_table_stats_latch_key;
#endif /* UNIV_PFS_RWLOCK */


#ifndef UNIV_PFS_RWLOCK
/******************************************************************//**
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed.
if MySQL performance schema is enabled and "UNIV_PFS_RWLOCK" is
defined, the rwlock are instrumented with performance schema probes. */
# ifdef UNIV_DEBUG
#  ifdef UNIV_SYNC_DEBUG
#   define rw_lock_create(K, L, level)				\
	rw_lock_create_func((L), (level), #L, __FILE__, __LINE__)
#  else	/* UNIV_SYNC_DEBUG */
#   define rw_lock_create(K, L, level)				\
	rw_lock_create_func((L), #L, __FILE__, __LINE__)
#  endif/* UNIV_SYNC_DEBUG */
# else /* UNIV_DEBUG */
#  define rw_lock_create(K, L, level)				\
	rw_lock_create_func((L), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

/**************************************************************//**
NOTE! The following macros should be used in rw locking and
unlocking, not the corresponding function. */

# define rw_lock_s_lock(M)					\
	rw_lock_s_lock_func((M), 0, __FILE__, __LINE__)

# define rw_lock_s_lock_inline(M, P, F, L)			\
	rw_lock_s_lock_func((M), (P), (F), (L))

# define rw_lock_s_lock_gen(M, P)				\
	rw_lock_s_lock_func((M), (P), __FILE__, __LINE__)

# define rw_lock_s_lock_nowait(M, F, L)				\
	rw_lock_s_lock_low((M), 0, (F), (L))

# ifdef UNIV_SYNC_DEBUG
#  define rw_lock_s_unlock_gen(L, P)	rw_lock_s_unlock_func(P, L)
# else
#  define rw_lock_s_unlock_gen(L, P)	rw_lock_s_unlock_func(L)
# endif


# define rw_lock_x_lock(M)					\
	rw_lock_x_lock_func((M), 0, __FILE__, __LINE__)

# define rw_lock_x_lock_inline(M, P, F, L)			\
	rw_lock_x_lock_func((M), (P), (F), (L))

# define rw_lock_x_lock_gen(M, P)				\
	rw_lock_x_lock_func((M), (P), __FILE__, __LINE__)

# define rw_lock_x_lock_nowait(M)				\
	rw_lock_x_lock_func_nowait((M), __FILE__, __LINE__)

# define rw_lock_x_lock_func_nowait_inline(M, F, L)		\
	rw_lock_x_lock_func_nowait((M), (F), (L))

# ifdef UNIV_SYNC_DEBUG
#  define rw_lock_x_unlock_gen(L, P)	rw_lock_x_unlock_func(P, L)
# else
#  define rw_lock_x_unlock_gen(L, P)	rw_lock_x_unlock_func(L)
# endif

# define rw_lock_free(M)		rw_lock_free_func(M)

#else /* !UNIV_PFS_RWLOCK */

/* Following macros point to Performance Schema instrumented functions. */
# ifdef UNIV_DEBUG
#  ifdef UNIV_SYNC_DEBUG
#   define rw_lock_create(K, L, level)				\
	pfs_rw_lock_create_func((K), (L), (level), #L, __FILE__, __LINE__)
#  else	/* UNIV_SYNC_DEBUG */
#   define rw_lock_create(K, L, level)				\
	pfs_rw_lock_create_func((K), (L), #L, __FILE__, __LINE__)
#  endif/* UNIV_SYNC_DEBUG */
# else	/* UNIV_DEBUG */
#  define rw_lock_create(K, L, level)				\
	pfs_rw_lock_create_func((K), (L), __FILE__, __LINE__)
# endif	/* UNIV_DEBUG */

/******************************************************************
NOTE! The following macros should be used in rw locking and
unlocking, not the corresponding function. */

# define rw_lock_s_lock(M)					\
	pfs_rw_lock_s_lock_func((M), 0, __FILE__, __LINE__)

# define rw_lock_s_lock_inline(M, P, F, L)			\
	pfs_rw_lock_s_lock_func((M), (P), (F), (L))

# define rw_lock_s_lock_gen(M, P)				\
	pfs_rw_lock_s_lock_func((M), (P), __FILE__, __LINE__)

# define rw_lock_s_lock_nowait(M, F, L)				\
	pfs_rw_lock_s_lock_low((M), 0, (F), (L))

# ifdef UNIV_SYNC_DEBUG
#  define rw_lock_s_unlock_gen(L, P)	pfs_rw_lock_s_unlock_func(P, L)
# else
#  define rw_lock_s_unlock_gen(L, P)	pfs_rw_lock_s_unlock_func(L)
# endif

# define rw_lock_x_lock(M)					\
	pfs_rw_lock_x_lock_func((M), 0, __FILE__, __LINE__)

# define rw_lock_x_lock_inline(M, P, F, L)			\
	pfs_rw_lock_x_lock_func((M), (P), (F), (L))

# define rw_lock_x_lock_gen(M, P)				\
	pfs_rw_lock_x_lock_func((M), (P), __FILE__, __LINE__)

# define rw_lock_x_lock_nowait(M)				\
	pfs_rw_lock_x_lock_func_nowait((M), __FILE__, __LINE__)

# define rw_lock_x_lock_func_nowait_inline(M, F, L)		\
	pfs_rw_lock_x_lock_func_nowait((M), (F), (L))

# ifdef UNIV_SYNC_DEBUG
#  define rw_lock_x_unlock_gen(L, P)	pfs_rw_lock_x_unlock_func(P, L)
# else
#  define rw_lock_x_unlock_gen(L, P)	pfs_rw_lock_x_unlock_func(L)
# endif

# define rw_lock_free(M)		pfs_rw_lock_free_func(M)

#endif /* UNIV_PFS_RWLOCK */

#define rw_lock_s_unlock(L)		rw_lock_s_unlock_gen(L, 0)
#define rw_lock_x_unlock(L)		rw_lock_x_unlock_gen(L, 0)

/******************************************************************//**
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
UNIV_INTERN
void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/*!< in: pointer to memory */
#ifdef UNIV_DEBUG
# ifdef UNIV_SYNC_DEBUG
	ulint		level,		/*!< in: level */
# endif /* UNIV_SYNC_DEBUG */
	const char*	cmutex_name,	/*!< in: mutex name */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline);		/*!< in: file line where created */
/******************************************************************//**
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */
UNIV_INTERN
void
rw_lock_free_func(
/*==============*/
	rw_lock_t*	lock);	/*!< in: rw-lock */
#ifdef UNIV_DEBUG
/******************************************************************//**
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks.
@return	TRUE */
UNIV_INTERN
ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock);	/*!< in: rw-lock */
#endif /* UNIV_DEBUG */
/******************************************************************//**
Low-level function which tries to lock an rw-lock in s-mode. Performs no
spinning.
@return	TRUE if success */
UNIV_INLINE
ibool
rw_lock_s_lock_low(
/*===============*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass __attribute__((unused)),
				/*!< in: pass value; != 0, if the lock will be
				passed to another thread to unlock */
	const char*	file_name, /*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
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
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread if the lock can be
obtained immediately.
@return	TRUE if success */
UNIV_INLINE
ibool
rw_lock_x_lock_func_nowait(
/*=======================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Releases a shared mode lock. */
UNIV_INLINE
void
rw_lock_s_unlock_func(
/*==================*/
#ifdef UNIV_SYNC_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	rw_lock_t*	lock);	/*!< in/out: rw-lock */

/******************************************************************//**
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
UNIV_INTERN
void
rw_lock_x_lock_func(
/*================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Releases an exclusive mode lock. */
UNIV_INLINE
void
rw_lock_x_unlock_func(
/*==================*/
#ifdef UNIV_SYNC_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif
	rw_lock_t*	lock);	/*!< in/out: rw-lock */


/******************************************************************//**
Low-level function which locks an rw-lock in s-mode when we know that it
is possible and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
UNIV_INLINE
void
rw_lock_s_lock_direct(
/*==================*/
	rw_lock_t*	lock,		/*!< in/out: rw-lock */
	const char*	file_name,	/*!< in: file name where requested */
	ulint		line);		/*!< in: line where lock requested */
/******************************************************************//**
Low-level function which locks an rw-lock in x-mode when we know that it
is not locked and none else is currently accessing the rw-lock structure.
Then we can do the locking without reserving the mutex. */
UNIV_INLINE
void
rw_lock_x_lock_direct(
/*==================*/
	rw_lock_t*	lock,		/*!< in/out: rw-lock */
	const char*	file_name,	/*!< in: file name where requested */
	ulint		line);		/*!< in: line where lock requested */
/******************************************************************//**
This function is used in the insert buffer to move the ownership of an
x-latch on a buffer frame to the current thread. The x-latch was set by
the buffer read operation and it protected the buffer frame while the
read was done. The ownership is moved because we want that the current
thread is able to acquire a second x-latch which is stored in an mtr.
This, in turn, is needed to pass the debug checks of index page
operations. */
UNIV_INTERN
void
rw_lock_x_lock_move_ownership(
/*==========================*/
	rw_lock_t*	lock);	/*!< in: lock which was x-locked in the
				buffer read */
/******************************************************************//**
Releases a shared mode lock when we know there are no waiters and none
else will access the lock during the time this function is executed. */
UNIV_INLINE
void
rw_lock_s_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Releases an exclusive mode lock when we know there are no waiters, and
none else will access the lock durint the time this function is executed. */
UNIV_INLINE
void
rw_lock_x_unlock_direct(
/*====================*/
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Returns the value of writer_count for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call.
@return	value of writer_count */
UNIV_INLINE
ulint
rw_lock_get_x_lock_count(
/*=====================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/********************************************************************//**
Check if there are threads waiting for the rw-lock.
@return	1 if waiters, 0 otherwise */
UNIV_INLINE
ulint
rw_lock_get_waiters(
/*================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Returns the write-status of the lock - this function made more sense
with the old rw_lock implementation.
@return	RW_LOCK_NOT_LOCKED, RW_LOCK_EX, RW_LOCK_WAIT_EX */
UNIV_INLINE
ulint
rw_lock_get_writer(
/*===============*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Returns the number of readers.
@return	number of readers */
UNIV_INLINE
ulint
rw_lock_get_reader_count(
/*=====================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Decrements lock_word the specified amount if it is greater than 0.
This is used by both s_lock and x_lock operations.
@return	TRUE if decr occurs */
UNIV_INLINE
ibool
rw_lock_lock_word_decr(
/*===================*/
	rw_lock_t*	lock,		/*!< in/out: rw-lock */
	ulint		amount);	/*!< in: amount to decrement */
/******************************************************************//**
Increments lock_word the specified amount and returns new value.
@return	lock->lock_word after increment */
UNIV_INLINE
lint
rw_lock_lock_word_incr(
/*===================*/
	rw_lock_t*	lock,		/*!< in/out: rw-lock */
	ulint		amount);	/*!< in: amount to increment */
/******************************************************************//**
This function sets the lock->writer_thread and lock->recursive fields.
For platforms where we are using atomic builtins instead of lock->mutex
it sets the lock->writer_thread field using atomics to ensure memory
ordering. Note that it is assumed that the caller of this function
effectively owns the lock i.e.: nobody else is allowed to modify
lock->writer_thread at this point in time.
The protocol is that lock->writer_thread MUST be updated BEFORE the
lock->recursive flag is set. */
UNIV_INLINE
void
rw_lock_set_writer_id_and_recursion_flag(
/*=====================================*/
	rw_lock_t*	lock,		/*!< in/out: lock to work on */
	ibool		recursive);	/*!< in: TRUE if recursion
					allowed */
#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
UNIV_INTERN
ibool
rw_lock_own(
/*========*/
	rw_lock_t*	lock,		/*!< in: rw-lock */
	ulint		lock_type)	/*!< in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
	__attribute__((warn_unused_result));
#endif /* UNIV_SYNC_DEBUG */
/******************************************************************//**
Checks if somebody has locked the rw-lock in the specified mode. */
UNIV_INTERN
ibool
rw_lock_is_locked(
/*==============*/
	rw_lock_t*	lock,		/*!< in: rw-lock */
	ulint		lock_type);	/*!< in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
#ifdef UNIV_SYNC_DEBUG
/***************************************************************//**
Prints debug info of an rw-lock. */
UNIV_INTERN
void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock);	/*!< in: rw-lock */
/***************************************************************//**
Prints debug info of currently locked rw-locks. */
UNIV_INTERN
void
rw_lock_list_print_info(
/*====================*/
	FILE*	file);		/*!< in: file where to print */
/***************************************************************//**
Returns the number of currently locked rw-locks.
Works only in the debug version.
@return	number of locked rw-locks */
UNIV_INTERN
ulint
rw_lock_n_locked(void);
/*==================*/

/*#####################################################################*/

/******************************************************************//**
Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
UNIV_INTERN
void
rw_lock_debug_mutex_enter(void);
/*===========================*/
/******************************************************************//**
Releases the debug mutex. */
UNIV_INTERN
void
rw_lock_debug_mutex_exit(void);
/*==========================*/
/*********************************************************************//**
Prints info of a debug struct. */
UNIV_INTERN
void
rw_lock_debug_print(
/*================*/
	FILE*			f,	/*!< in: output stream */
	rw_lock_debug_t*	info);	/*!< in: debug struct */
#endif /* UNIV_SYNC_DEBUG */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! */

/** The structure used in the spin lock implementation of a read-write
lock. Several threads may have a shared lock simultaneously in this
lock, but only one writer may have an exclusive lock, in which case no
shared locks are allowed. To prevent starving of a writer blocked by
readers, a writer may queue for x-lock by decrementing lock_word: no
new readers will be let in while the thread waits for readers to
exit. */
struct rw_lock_struct {
	volatile lint	lock_word;
				/*!< Holds the state of the lock. */
	volatile ulint	waiters;/*!< 1: there are waiters */
	volatile ibool	recursive;/*!< Default value FALSE which means the lock
				is non-recursive. The value is typically set
				to TRUE making normal rw_locks recursive. In
				case of asynchronous IO, when a non-zero
				value of 'pass' is passed then we keep the
				lock non-recursive.
				This flag also tells us about the state of
				writer_thread field. If this flag is set
				then writer_thread MUST contain the thread
				id of the current x-holder or wait-x thread.
				This flag must be reset in x_unlock
				functions before incrementing the lock_word */
	volatile os_thread_id_t	writer_thread;
				/*!< Thread id of writer thread. Is only
				guaranteed to have sane and non-stale
				value iff recursive flag is set. */
	os_event_t	event;	/*!< Used by sync0arr.c for thread queueing */
	os_event_t	wait_ex_event;
				/*!< Event for next-writer to wait on. A thread
				must decrement lock_word before waiting. */
#ifndef INNODB_RW_LOCKS_USE_ATOMICS
	mutex_t	mutex;		/*!< The mutex protecting rw_lock_struct */
#endif /* INNODB_RW_LOCKS_USE_ATOMICS */

	UT_LIST_NODE_T(rw_lock_t) list;
				/*!< All allocated rw locks are put into a
				list */
#ifdef UNIV_SYNC_DEBUG
	UT_LIST_BASE_NODE_T(rw_lock_debug_t) debug_list;
				/*!< In the debug version: pointer to the debug
				info list of the lock */
	ulint	level;		/*!< Level in the global latching order. */
#endif /* UNIV_SYNC_DEBUG */
#ifdef UNIV_PFS_RWLOCK
	struct PSI_rwlock *pfs_psi;/*!< The instrumentation hook */
#endif
	ulint count_os_wait;	/*!< Count of os_waits. May not be accurate */
	const char*	cfile_name;/*!< File name where lock created */
        /* last s-lock file/line is not guaranteed to be correct */
	const char*	last_s_file_name;/*!< File name where last s-locked */
	const char*	last_x_file_name;/*!< File name where last x-locked */
	ibool		writer_is_wait_ex;
				/*!< This is TRUE if the writer field is
				RW_LOCK_WAIT_EX; this field is located far
				from the memory update hotspot fields which
				are at the start of this struct, thus we can
				peek this field without causing much memory
				bus traffic */
	unsigned	cline:14;	/*!< Line where created */
	unsigned	last_s_line:14;	/*!< Line number where last time s-locked */
	unsigned	last_x_line:14;	/*!< Line number where last time x-locked */
#ifdef UNIV_DEBUG
	ulint	magic_n;	/*!< RW_LOCK_MAGIC_N */
/** Value of rw_lock_struct::magic_n */
#define	RW_LOCK_MAGIC_N	22643
#endif /* UNIV_DEBUG */
};

#ifdef UNIV_SYNC_DEBUG
/** The structure for storing debug info of an rw-lock.  All access to this
structure must be protected by rw_lock_debug_mutex_enter(). */
struct	rw_lock_debug_struct {

	os_thread_id_t thread_id;  /*!< The thread id of the thread which
				locked the rw-lock */
	ulint	pass;		/*!< Pass value given in the lock operation */
	ulint	lock_type;	/*!< Type of the lock: RW_LOCK_EX,
				RW_LOCK_SHARED, RW_LOCK_WAIT_EX */
	const char*	file_name;/*!< File name where the lock was obtained */
	ulint	line;		/*!< Line where the rw-lock was locked */
	UT_LIST_NODE_T(rw_lock_debug_t) list;
				/*!< Debug structs are linked in a two-way
				list */
};
#endif /* UNIV_SYNC_DEBUG */

/* For performance schema instrumentation, a new set of rwlock
wrap functions are created if "UNIV_PFS_RWLOCK" is defined.
The instrumentations are not planted directly into original
functions, so that we keep the underlying function as they
are. And in case, user wants to "take out" some rwlock from
instrumentation even if performance schema (UNIV_PFS_RWLOCK)
is defined, they can do so by reinstating APIs directly link to
original underlying functions.
The instrumented function names have prefix of "pfs_rw_lock_" vs.
original name prefix of "rw_lock_". Following are list of functions
that have been instrumented:

rw_lock_create()
rw_lock_x_lock()
rw_lock_x_lock_gen()
rw_lock_x_lock_nowait()
rw_lock_x_unlock_gen()
rw_lock_s_lock()
rw_lock_s_lock_gen()
rw_lock_s_lock_nowait()
rw_lock_s_unlock_gen()
rw_lock_free()

Two function APIs rw_lock_x_unlock_direct() and rw_lock_s_unlock_direct()
do not have any caller/user, they are not instrumented.
*/

#ifdef UNIV_PFS_RWLOCK
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_create_func()
NOTE! Please use the corresponding macro rw_lock_create(), not
directly this function! */
UNIV_INLINE
void
pfs_rw_lock_create_func(
/*====================*/
	PSI_rwlock_key  key,		/*!< in: key registered with
					performance schema */
	rw_lock_t*	lock,		/*!< in: rw lock */
#ifdef UNIV_DEBUG
# ifdef UNIV_SYNC_DEBUG
	ulint		level,		/*!< in: level */
# endif /* UNIV_SYNC_DEBUG */
	const char*	cmutex_name,	/*!< in: mutex name */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline);		/*!< in: file line where created */

/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_x_lock_func()
NOTE! Please use the corresponding macro rw_lock_x_lock(), not
directly this function! */
UNIV_INLINE
void
pfs_rw_lock_x_lock_func(
/*====================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for
rw_lock_x_lock_func_nowait()
NOTE! Please use the corresponding macro, not directly this function!
@return TRUE if success */
UNIV_INLINE
ibool
pfs_rw_lock_x_lock_func_nowait(
/*===========================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_s_lock_func()
NOTE! Please use the corresponding macro rw_lock_s_lock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_s_lock_func(
/*====================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_s_lock_func()
NOTE! Please use the corresponding macro rw_lock_s_lock(), not directly
this function!
@return TRUE if success */
UNIV_INLINE
ibool
pfs_rw_lock_s_lock_low(
/*===================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock will be passed to another
				thread to unlock */
	const char*	file_name, /*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_x_lock_func()
NOTE! Please use the corresponding macro rw_lock_x_lock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_x_lock_func(
/*====================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_s_unlock_func()
NOTE! Please use the corresponding macro rw_lock_s_unlock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_s_unlock_func(
/*======================*/
#ifdef UNIV_SYNC_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock may have been passed to another
				thread to unlock */
#endif
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_s_unlock_func()
NOTE! Please use the corresponding macro rw_lock_x_unlock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_x_unlock_func(
/*======================*/
#ifdef UNIV_SYNC_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock may have been passed to another
				thread to unlock */
#endif
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_free_func()
NOTE! Please use the corresponding macro rw_lock_free(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_free_func(
/*==================*/
	rw_lock_t*	lock);	/*!< in: rw-lock */
#endif  /* UNIV_PFS_RWLOCK */


#ifndef UNIV_NONINL
#include "sync0rw.ic"
#endif
#endif /* !UNIV_HOTBACKUP */

#endif
