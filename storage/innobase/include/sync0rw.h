/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.
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
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

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
#include "ut0counter.h"
#include "os0event.h"
#include "ut0mutex.h"

#endif /* !UNIV_HOTBACKUP */

/** Counters for RW locks. */
struct rw_lock_stats_t {
	typedef ib_counter_t<int64_t, IB_N_SLOTS> int64_counter_t;

	/** number of spin waits on rw-latches,
	resulted during shared (read) locks */
	int64_counter_t		rw_s_spin_wait_count;

	/** number of spin loop rounds on rw-latches,
	resulted during shared (read) locks */
	int64_counter_t		rw_s_spin_round_count;

	/** number of OS waits on rw-latches,
	resulted during shared (read) locks */
	int64_counter_t		rw_s_os_wait_count;

	/** number of spin waits on rw-latches,
	resulted during exclusive (write) locks */
	int64_counter_t		rw_x_spin_wait_count;

	/** number of spin loop rounds on rw-latches,
	resulted during exclusive (write) locks */
	int64_counter_t		rw_x_spin_round_count;

	/** number of OS waits on rw-latches,
	resulted during exclusive (write) locks */
	int64_counter_t		rw_x_os_wait_count;

	/** number of spin waits on rw-latches,
	resulted during sx locks */
	int64_counter_t		rw_sx_spin_wait_count;

	/** number of spin loop rounds on rw-latches,
	resulted during sx locks */
	int64_counter_t		rw_sx_spin_round_count;

	/** number of OS waits on rw-latches,
	resulted during sx locks */
	int64_counter_t		rw_sx_os_wait_count;
};

/* Latch types; these are used also in btr0btr.h and mtr0mtr.h: keep the
numerical values smaller than 30 (smaller than BTR_MODIFY_TREE and
MTR_MEMO_MODIFY) and the order of the numerical values like below! and they
should be 2pow value to be used also as ORed combination of flag. */
enum rw_lock_type_t {
	RW_S_LATCH = 1,
	RW_X_LATCH = 2,
	RW_SX_LATCH = 4,
	RW_NO_LATCH = 8
};

#ifndef UNIV_HOTBACKUP
/* We decrement lock_word by X_LOCK_DECR for each x_lock. It is also the
start value for the lock_word, meaning that it limits the maximum number
of concurrent read locks before the rw_lock breaks. */
/* We decrement lock_word by X_LOCK_HALF_DECR for sx_lock. */
#define X_LOCK_DECR		0x20000000
#define X_LOCK_HALF_DECR	0x10000000

struct rw_lock_t;

#ifdef UNIV_DEBUG
struct rw_lock_debug_t;
#endif /* UNIV_DEBUG */

typedef UT_LIST_BASE_NODE_T(rw_lock_t)	rw_lock_list_t;

extern rw_lock_list_t			rw_lock_list;
extern ib_mutex_t			rw_lock_list_mutex;

/** Counters for RW locks. */
extern rw_lock_stats_t	rw_lock_stats;

#ifndef UNIV_PFS_RWLOCK
/******************************************************************//**
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed.
if MySQL performance schema is enabled and "UNIV_PFS_RWLOCK" is
defined, the rwlock are instrumented with performance schema probes. */
# ifdef UNIV_DEBUG
#  define rw_lock_create(K, L, level)				\
	rw_lock_create_func((L), (level), #L, __FILE__, __LINE__)
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

# ifdef UNIV_DEBUG
#  define rw_lock_s_unlock_gen(L, P)	rw_lock_s_unlock_func(P, L)
# else
#  define rw_lock_s_unlock_gen(L, P)	rw_lock_s_unlock_func(L)
# endif /* UNIV_DEBUG */

#define rw_lock_sx_lock(L)					\
	rw_lock_sx_lock_func((L), 0, __FILE__, __LINE__)

#define rw_lock_sx_lock_inline(M, P, F, L)			\
	rw_lock_sx_lock_func((M), (P), (F), (L))

#define rw_lock_sx_lock_gen(M, P)				\
	rw_lock_sx_lock_func((M), (P), __FILE__, __LINE__)

#define rw_lock_sx_lock_nowait(M, P)				\
	rw_lock_sx_lock_low((M), (P), __FILE__, __LINE__)

# ifdef UNIV_DEBUG
#  define rw_lock_sx_unlock(L)		rw_lock_sx_unlock_func(0, L)
#  define rw_lock_sx_unlock_gen(L, P)	rw_lock_sx_unlock_func(P, L)
# else /* UNIV_DEBUG */
#  define rw_lock_sx_unlock(L)		rw_lock_sx_unlock_func(L)
#  define rw_lock_sx_unlock_gen(L, P)	rw_lock_sx_unlock_func(L)
# endif /* UNIV_DEBUG */

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

# ifdef UNIV_DEBUG
#  define rw_lock_x_unlock_gen(L, P)	rw_lock_x_unlock_func(P, L)
# else
#  define rw_lock_x_unlock_gen(L, P)	rw_lock_x_unlock_func(L)
# endif

# define rw_lock_free(M)		rw_lock_free_func(M)

#else /* !UNIV_PFS_RWLOCK */

/* Following macros point to Performance Schema instrumented functions. */
# ifdef UNIV_DEBUG
#   define rw_lock_create(K, L, level)				\
	pfs_rw_lock_create_func((K), (L), (level), #L, __FILE__, __LINE__)
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

# ifdef UNIV_DEBUG
#  define rw_lock_s_unlock_gen(L, P)	pfs_rw_lock_s_unlock_func(P, L)
# else
#  define rw_lock_s_unlock_gen(L, P)	pfs_rw_lock_s_unlock_func(L)
# endif

# define rw_lock_sx_lock(M)					\
	pfs_rw_lock_sx_lock_func((M), 0, __FILE__, __LINE__)

# define rw_lock_sx_lock_inline(M, P, F, L)			\
	pfs_rw_lock_sx_lock_func((M), (P), (F), (L))

# define rw_lock_sx_lock_gen(M, P)				\
	pfs_rw_lock_sx_lock_func((M), (P), __FILE__, __LINE__)

#define rw_lock_sx_lock_nowait(M, P)				\
	pfs_rw_lock_sx_lock_low((M), (P), __FILE__, __LINE__)

# ifdef UNIV_DEBUG
#  define rw_lock_sx_unlock(L)		pfs_rw_lock_sx_unlock_func(0, L)
#  define rw_lock_sx_unlock_gen(L, P)	pfs_rw_lock_sx_unlock_func(P, L)
# else
#  define rw_lock_sx_unlock(L)		pfs_rw_lock_sx_unlock_func(L)
#  define rw_lock_sx_unlock_gen(L, P)	pfs_rw_lock_sx_unlock_func(L)
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

# ifdef UNIV_DEBUG
#  define rw_lock_x_unlock_gen(L, P)	pfs_rw_lock_x_unlock_func(P, L)
# else
#  define rw_lock_x_unlock_gen(L, P)	pfs_rw_lock_x_unlock_func(L)
# endif

# define rw_lock_free(M)		pfs_rw_lock_free_func(M)

#endif /* !UNIV_PFS_RWLOCK */

#define rw_lock_s_unlock(L)		rw_lock_s_unlock_gen(L, 0)
#define rw_lock_x_unlock(L)		rw_lock_x_unlock_gen(L, 0)

/******************************************************************//**
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/*!< in: pointer to memory */
#ifdef UNIV_DEBUG
	latch_level_t	level,		/*!< in: level */
	const char*	cmutex_name,	/*!< in: mutex name */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline);		/*!< in: file line where created */
/******************************************************************//**
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */
void
rw_lock_free_func(
/*==============*/
	rw_lock_t*	lock);		/*!< in/out: rw-lock */
#ifdef UNIV_DEBUG
/******************************************************************//**
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks.
@return true */
bool
rw_lock_validate(
/*=============*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
#endif /* UNIV_DEBUG */
/******************************************************************//**
Low-level function which tries to lock an rw-lock in s-mode. Performs no
spinning.
@return TRUE if success */
UNIV_INLINE
ibool
rw_lock_s_lock_low(
/*===============*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass MY_ATTRIBUTE((unused)),
				/*!< in: pass value; != 0, if the lock will be
				passed to another thread to unlock */
	const char*	file_name, /*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode
for the current thread. If the rw-lock is locked in exclusive mode, or
there is an exclusive lock request waiting, the function spins a preset
time (controlled by srv_n_spin_wait_rounds), waiting for the lock, before
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
@return TRUE if success */
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
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif /* UNIV_DEBUG */
	rw_lock_t*	lock);	/*!< in/out: rw-lock */

/******************************************************************//**
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by srv_n_spin_wait_rounds), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
void
rw_lock_x_lock_func(
/*================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Low-level function for acquiring an sx lock.
@return FALSE if did not succeed, TRUE if success. */
ibool
rw_lock_sx_lock_low(
/*================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in SX mode for the current thread. If the rw-lock is locked
in exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single sx-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
void
rw_lock_sx_lock_func(
/*=================*/
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
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif /* UNIV_DEBUG */
	rw_lock_t*	lock);	/*!< in/out: rw-lock */

/******************************************************************//**
Releases an sx mode lock. */
UNIV_INLINE
void
rw_lock_sx_unlock_func(
/*===================*/
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the lock may have
				been passed to another thread to unlock */
#endif /* UNIV_DEBUG */
	rw_lock_t*	lock);	/*!< in/out: rw-lock */

/******************************************************************//**
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
	rw_lock_t*	lock);	/*!< in: lock which was x-locked in the
				buffer read */
/******************************************************************//**
Returns the value of writer_count for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call.
@return value of writer_count */
UNIV_INLINE
ulint
rw_lock_get_x_lock_count(
/*=====================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Returns the number of sx-lock for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call.
@return value of writer_count */
UNIV_INLINE
ulint
rw_lock_get_sx_lock_count(
/*======================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/********************************************************************//**
Check if there are threads waiting for the rw-lock.
@return 1 if waiters, 0 otherwise */
UNIV_INLINE
ulint
rw_lock_get_waiters(
/*================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Returns the write-status of the lock - this function made more sense
with the old rw_lock implementation.
@return RW_LOCK_NOT_LOCKED, RW_LOCK_X, RW_LOCK_X_WAIT, RW_LOCK_SX */
UNIV_INLINE
ulint
rw_lock_get_writer(
/*===============*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Returns the number of readers (s-locks).
@return number of readers */
UNIV_INLINE
ulint
rw_lock_get_reader_count(
/*=====================*/
	const rw_lock_t*	lock);	/*!< in: rw-lock */
/******************************************************************//**
Decrements lock_word the specified amount if it is greater than 0.
This is used by both s_lock and x_lock operations.
@return true if decr occurs */
UNIV_INLINE
bool
rw_lock_lock_word_decr(
/*===================*/
	rw_lock_t*	lock,		/*!< in/out: rw-lock */
	ulint		amount,		/*!< in: amount to decrement */
	lint		threshold);	/*!< in: threshold of judgement */
/******************************************************************//**
Increments lock_word the specified amount and returns new value.
@return lock->lock_word after increment */
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
	bool		recursive);	/*!< in: true if recursion
					allowed */
#ifdef UNIV_DEBUG
/******************************************************************//**
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
ibool
rw_lock_own(
/*========*/
	rw_lock_t*	lock,		/*!< in: rw-lock */
	ulint		lock_type)	/*!< in: lock type: RW_LOCK_S,
					RW_LOCK_X */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************************//**
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
bool
rw_lock_own_flagged(
/*================*/
	const rw_lock_t*	lock,	/*!< in: rw-lock */
	rw_lock_flags_t		flags)	/*!< in: specify lock types with
					OR of the rw_lock_flag_t values */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */
/******************************************************************//**
Checks if somebody has locked the rw-lock in the specified mode.
@return true if locked */
bool
rw_lock_is_locked(
/*==============*/
	rw_lock_t*	lock,		/*!< in: rw-lock */
	ulint		lock_type);	/*!< in: lock type: RW_LOCK_S,
					RW_LOCK_X or RW_LOCK_SX */
#ifdef UNIV_DEBUG
/***************************************************************//**
Prints debug info of an rw-lock. */
void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock);		/*!< in: rw-lock */
/***************************************************************//**
Prints debug info of currently locked rw-locks. */
void
rw_lock_list_print_info(
/*====================*/
	FILE*		file);		/*!< in: file where to print */
/***************************************************************//**
Returns the number of currently locked rw-locks.
Works only in the debug version.
@return number of locked rw-locks */
ulint
rw_lock_n_locked(void);
/*==================*/

/*#####################################################################*/

/*********************************************************************//**
Prints info of a debug struct. */
void
rw_lock_debug_print(
/*================*/
	FILE*			f,	/*!< in: output stream */
	const rw_lock_debug_t*	info);	/*!< in: debug struct */
#endif /* UNIV_DEBUG */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! */

/** The structure used in the spin lock implementation of a read-write
lock. Several threads may have a shared lock simultaneously in this
lock, but only one writer may have an exclusive lock, in which case no
shared locks are allowed. To prevent starving of a writer blocked by
readers, a writer may queue for x-lock by decrementing lock_word: no
new readers will be let in while the thread waits for readers to
exit. */

struct rw_lock_t
#ifdef UNIV_DEBUG
	: public latch_t
#endif /* UNIV_DEBUG */
{
	/** Holds the state of the lock. */
	volatile lint	lock_word;

	/** 1: there are waiters */
	volatile ulint	waiters;

	/** Default value FALSE which means the lock is non-recursive.
	The value is typically set to TRUE making normal rw_locks recursive.
	In case of asynchronous IO, when a non-zero value of 'pass' is
	passed then we keep the lock non-recursive.

	This flag also tells us about the state of writer_thread field.
	If this flag is set then writer_thread MUST contain the thread
	id of the current x-holder or wait-x thread.  This flag must be
	reset in x_unlock functions before incrementing the lock_word */
	volatile bool	recursive;

	/** number of granted SX locks. */
	volatile ulint	sx_recursive;

	/** This is TRUE if the writer field is RW_LOCK_X_WAIT; this field
	is located far from the memory update hotspot fields which are at
	the start of this struct, thus we can peek this field without
	causing much memory bus traffic */
	bool		writer_is_wait_ex;

	/** Thread id of writer thread. Is only guaranteed to have sane
	and non-stale value iff recursive flag is set. */
	volatile os_thread_id_t	writer_thread;

	/** Used by sync0arr.cc for thread queueing */
	os_event_t	event;

	/** Event for next-writer to wait on. A thread must decrement
	lock_word before waiting. */
	os_event_t	wait_ex_event;

	/** File name where lock created */
	const char*	cfile_name;

	/** last s-lock file/line is not guaranteed to be correct */
	const char*	last_s_file_name;

	/** File name where last x-locked */
	const char*	last_x_file_name;

	/** Line where created */
	unsigned	cline:13;

	/** If 1 then the rw-lock is a block lock */
	unsigned	is_block_lock:1;

	/** Line number where last time s-locked */
	unsigned	last_s_line:14;

	/** Line number where last time x-locked */
	unsigned	last_x_line:14;

	/** Count of os_waits. May not be accurate */
	uint32_t	count_os_wait;

	/** All allocated rw locks are put into a list */
	UT_LIST_NODE_T(rw_lock_t) list;

#ifdef UNIV_PFS_RWLOCK
	/** The instrumentation hook */
	struct PSI_rwlock*	pfs_psi;
#endif /* UNIV_PFS_RWLOCK */

#ifndef INNODB_RW_LOCKS_USE_ATOMICS
	/** The mutex protecting rw_lock_t */
	mutable ib_mutex_t mutex;
#endif /* INNODB_RW_LOCKS_USE_ATOMICS */

#ifdef UNIV_DEBUG
/** Value of rw_lock_t::magic_n */
# define RW_LOCK_MAGIC_N	22643

	/** Constructor */
	rw_lock_t()
	{
		magic_n = RW_LOCK_MAGIC_N;
	}

	/** Destructor */
	virtual ~rw_lock_t()
	{
		ut_ad(magic_n == RW_LOCK_MAGIC_N);
		magic_n = 0;
	}

	virtual std::string to_string() const;
	virtual std::string locked_from() const;

	/** For checking memory corruption. */
	ulint		magic_n;

	/** In the debug version: pointer to the debug info list of the lock */
	UT_LIST_BASE_NODE_T(rw_lock_debug_t) debug_list;

	/** Level in the global latching order. */
	latch_level_t	level;

#endif /* UNIV_DEBUG */

};
#ifdef UNIV_DEBUG
/** The structure for storing debug info of an rw-lock.  All access to this
structure must be protected by rw_lock_debug_mutex_enter(). */
struct	rw_lock_debug_t {

	os_thread_id_t thread_id;  /*!< The thread id of the thread which
				locked the rw-lock */
	ulint	pass;		/*!< Pass value given in the lock operation */
	ulint	lock_type;	/*!< Type of the lock: RW_LOCK_X,
				RW_LOCK_S, RW_LOCK_X_WAIT */
	const char*	file_name;/*!< File name where the lock was obtained */
	ulint	line;		/*!< Line where the rw-lock was locked */
	UT_LIST_NODE_T(rw_lock_debug_t) list;
				/*!< Debug structs are linked in a two-way
				list */
};
#endif /* UNIV_DEBUG */

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
rw_lock_sx_lock()
rw_lock_sx_unlock_gen()
rw_lock_free()
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
	mysql_pfs_key_t	key,		/*!< in: key registered with
					performance schema */
	rw_lock_t*	lock,		/*!< in: rw lock */
#ifdef UNIV_DEBUG
	latch_level_t	level,		/*!< in: level */
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
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock may have been passed to another
				thread to unlock */
#endif /* UNIV_DEBUG */
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_x_unlock_func()
NOTE! Please use the corresponding macro rw_lock_x_unlock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_x_unlock_func(
/*======================*/
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock may have been passed to another
				thread to unlock */
#endif /* UNIV_DEBUG */
	rw_lock_t*	lock);	/*!< in/out: rw-lock */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_sx_lock_func()
NOTE! Please use the corresponding macro rw_lock_sx_lock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_sx_lock_func(
/*====================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_sx_lock_nowait()
NOTE! Please use the corresponding macro, not directly
this function! */
UNIV_INLINE
ibool
pfs_rw_lock_sx_lock_low(
/*================*/
	rw_lock_t*	lock,	/*!< in: pointer to rw-lock */
	ulint		pass,	/*!< in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/*!< in: file name where lock requested */
	ulint		line);	/*!< in: line where requested */
/******************************************************************//**
Performance schema instrumented wrap function for rw_lock_sx_unlock_func()
NOTE! Please use the corresponding macro rw_lock_sx_unlock(), not directly
this function! */
UNIV_INLINE
void
pfs_rw_lock_sx_unlock_func(
/*======================*/
#ifdef UNIV_DEBUG
	ulint		pass,	/*!< in: pass value; != 0, if the
				lock may have been passed to another
				thread to unlock */
#endif /* UNIV_DEBUG */
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
#endif /* !UNIV_NONINL */

#endif /* !UNIV_HOTBACKUP */

#endif /* sync0rw.h */
