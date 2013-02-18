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
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file sync/sync0sync.cc
Mutex, the basic synchronization primitive

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#include "sync0sync.h"
#ifdef UNIV_NONINL
#include "sync0sync.ic"
#endif

#include "sync0rw.h"
#include "buf0buf.h"
#include "srv0srv.h"
#include "buf0types.h"
#include "os0sync.h" /* for HAVE_ATOMIC_BUILTINS */
#ifdef UNIV_SYNC_DEBUG
# include "srv0start.h" /* srv_is_being_started */
#endif /* UNIV_SYNC_DEBUG */
#include "ha_prototypes.h"

/*
	REASONS FOR IMPLEMENTING THE SPIN LOCK MUTEX
	============================================

Semaphore operations in operating systems are slow: Solaris on a 1993 Sparc
takes 3 microseconds (us) for a lock-unlock pair and Windows NT on a 1995
Pentium takes 20 microseconds for a lock-unlock pair. Therefore, we have to
implement our own efficient spin lock mutex. Future operating systems may
provide efficient spin locks, but we cannot count on that.

Another reason for implementing a spin lock is that on multiprocessor systems
it can be more efficient for a processor to run a loop waiting for the
semaphore to be released than to switch to a different thread. A thread switch
takes 25 us on both platforms mentioned above. See Gray and Reuter's book
Transaction processing for background.

How long should the spin loop last before suspending the thread? On a
uniprocessor, spinning does not help at all, because if the thread owning the
mutex is not executing, it cannot be released. Spinning actually wastes
resources.

On a multiprocessor, we do not know if the thread owning the mutex is
executing or not. Thus it would make sense to spin as long as the operation
guarded by the mutex would typically last assuming that the thread is
executing. If the mutex is not released by that time, we may assume that the
thread owning the mutex is not executing and suspend the waiting thread.

A typical operation (where no i/o involved) guarded by a mutex or a read-write
lock may last 1 - 20 us on the current Pentium platform. The longest
operations are the binary searches on an index node.

We conclude that the best choice is to set the spin time at 20 us. Then the
system should work well on a multiprocessor. On a uniprocessor we have to
make sure that thread swithches due to mutex collisions are not frequent,
i.e., they do not happen every 100 us or so, because that wastes too much
resources. If the thread switches are not frequent, the 20 us wasted in spin
loop is not too much.

Empirical studies on the effect of spin time should be done for different
platforms.


	IMPLEMENTATION OF THE MUTEX
	===========================

For background, see Curt Schimmel's book on Unix implementation on modern
architectures. The key points in the implementation are atomicity and
serialization of memory accesses. The test-and-set instruction (XCHG in
Pentium) must be atomic. As new processors may have weak memory models, also
serialization of memory references may be necessary. The successor of Pentium,
P6, has at least one mode where the memory model is weak. As far as we know,
in Pentium all memory accesses are serialized in the program order and we do
not have to worry about the memory model. On other processors there are
special machine instructions called a fence, memory barrier, or storage
barrier (STBAR in Sparc), which can be used to serialize the memory accesses
to happen in program order relative to the fence instruction.

Leslie Lamport has devised a "bakery algorithm" to implement a mutex without
the atomic test-and-set, but his algorithm should be modified for weak memory
models. We do not use Lamport's algorithm, because we guess it is slower than
the atomic test-and-set.

Our mutex implementation works as follows: After that we perform the atomic
test-and-set instruction on the memory word. If the test returns zero, we
know we got the lock first. If the test returns not zero, some other thread
was quicker and got the lock: then we spin in a loop reading the memory word,
waiting it to become zero. It is wise to just read the word in the loop, not
perform numerous test-and-set instructions, because they generate memory
traffic between the cache and the main memory. The read loop can just access
the cache, saving bus bandwidth.

If we cannot acquire the mutex lock in the specified time, we reserve a cell
in the wait array, set the waiters byte in the mutex to 1. To avoid a race
condition, after setting the waiters byte and before suspending the waiting
thread, we still have to check that the mutex is reserved, because it may
have happened that the thread which was holding the mutex has just released
it and did not see the waiters byte set to 1, a case which would lead the
other thread to an infinite wait.

LEMMA 1: After a thread resets the event of a mutex (or rw_lock), some
=======
thread will eventually call os_event_set() on that particular event.
Thus no infinite wait is possible in this case.

Proof:	After making the reservation the thread sets the waiters field in the
mutex to 1. Then it checks that the mutex is still reserved by some thread,
or it reserves the mutex for itself. In any case, some thread (which may be
also some earlier thread, not necessarily the one currently holding the mutex)
will set the waiters field to 0 in mutex_exit, and then call
os_event_set() with the mutex as an argument.
Q.E.D.

LEMMA 2: If an os_event_set() call is made after some thread has called
=======
the os_event_reset() and before it starts wait on that event, the call
will not be lost to the second thread. This is true even if there is an
intervening call to os_event_reset() by another thread.
Thus no infinite wait is possible in this case.

Proof (non-windows platforms): os_event_reset() returns a monotonically
increasing value of signal_count. This value is increased at every
call of os_event_set() If thread A has called os_event_reset() followed
by thread B calling os_event_set() and then some other thread C calling
os_event_reset(), the is_set flag of the event will be set to FALSE;
but now if thread A calls os_event_wait_low() with the signal_count
value returned from the earlier call of os_event_reset(), it will
return immediately without waiting.
Q.E.D.

Proof (windows): If there is a writer thread which is forced to wait for
the lock, it may be able to set the state of rw_lock to RW_LOCK_WAIT_EX
The design of rw_lock ensures that there is one and only one thread
that is able to change the state to RW_LOCK_WAIT_EX and this thread is
guaranteed to acquire the lock after it is released by the current
holders and before any other waiter gets the lock.
On windows this thread waits on a separate event i.e.: wait_ex_event.
Since only one thread can wait on this event there is no chance
of this event getting reset before the writer starts wait on it.
Therefore, this thread is guaranteed to catch the os_set_event()
signalled unconditionally at the release of the lock.
Q.E.D. */

/* Number of spin waits on mutexes: for performance monitoring */

/** The number of iterations in the mutex_spin_wait() spin loop.
Intended for performance monitoring. */
static ib_counter_t<ib_int64_t, IB_N_SLOTS>	mutex_spin_round_count;
/** The number of mutex_spin_wait() calls.  Intended for
performance monitoring. */
static ib_counter_t<ib_int64_t, IB_N_SLOTS>	mutex_spin_wait_count;
/** The number of OS waits in mutex_spin_wait().  Intended for
performance monitoring. */
static ib_counter_t<ib_int64_t, IB_N_SLOTS>	mutex_os_wait_count;
/** The number of mutex_exit() calls. Intended for performance
monitoring. */
UNIV_INTERN ib_int64_t			mutex_exit_count;

/** This variable is set to TRUE when sync_init is called */
UNIV_INTERN ibool	sync_initialized	= FALSE;

#ifdef UNIV_SYNC_DEBUG
/** An acquired mutex or rw-lock and its level in the latching order */
struct sync_level_t;
/** Mutexes or rw-locks held by a thread */
struct sync_thread_t;

/** The latch levels currently owned by threads are stored in this data
structure; the size of this array is OS_THREAD_MAX_N */

UNIV_INTERN sync_thread_t*	sync_thread_level_arrays;

/** Mutex protecting sync_thread_level_arrays */
UNIV_INTERN ib_mutex_t		sync_thread_mutex;

# ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_PFS_MUTEX */
#endif /* UNIV_SYNC_DEBUG */

/** Global list of database mutexes (not OS mutexes) created. */
UNIV_INTERN ut_list_base_node_t  mutex_list;

/** Mutex protecting the mutex_list variable */
UNIV_INTERN ib_mutex_t mutex_list_mutex;

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	mutex_list_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_SYNC_DEBUG
/** Latching order checks start when this is set TRUE */
UNIV_INTERN ibool	sync_order_checks_on	= FALSE;

/** Number of slots reserved for each OS thread in the sync level array */
static const ulint SYNC_THREAD_N_LEVELS = 10000;

/** Array for tracking sync levels per thread. */
struct sync_arr_t {
	ulint		in_use;		/*!< Number of active cells */
	ulint		n_elems;	/*!< Number of elements in the array */
	ulint		max_elems;	/*!< Maximum elements */
	ulint		next_free;	/*!< ULINT_UNDEFINED or index of next
					free slot */
	sync_level_t*	elems;		/*!< Array elements */
};

/** Mutexes or rw-locks held by a thread */
struct sync_thread_t{
	os_thread_id_t	id;		/*!< OS thread id */
	sync_arr_t*	levels;		/*!< level array for this thread; if
					this is NULL this slot is unused */
};

/** An acquired mutex or rw-lock and its level in the latching order */
struct sync_level_t{
	void*		latch;		/*!< pointer to a mutex or an
					rw-lock; NULL means that
					the slot is empty */
	ulint		level;		/*!< level of the latch in the
					latching order. This field is
					overloaded to serve as a node in a
					linked list of free nodes too. When
					latch == NULL then this will contain
					the ordinal value of the next free
					element */
	ulint		count;		/*!< Numbe of times this latch has
					been locked at this level.  Allows
					for recursive locking */
};
#endif /* UNIV_SYNC_DEBUG */

/******************************************************************//**
Creates, or rather, initializes a mutex object in a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */
UNIV_INTERN
void
mutex_create_func(
/*==============*/
	ib_mutex_t*	mutex,		/*!< in: pointer to memory */
#ifdef UNIV_DEBUG
	const char*	cmutex_name,	/*!< in: mutex name */
# ifdef UNIV_SYNC_DEBUG
	ulint		level,		/*!< in: level */
# endif /* UNIV_SYNC_DEBUG */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/*!< in: file name where created */
	ulint		cline)		/*!< in: file line where created */
{
#if defined(HAVE_ATOMIC_BUILTINS)
	mutex_reset_lock_word(mutex);
#else
	os_fast_mutex_init(PFS_NOT_INSTRUMENTED, &mutex->os_fast_mutex);
	mutex->lock_word = 0;
#endif
	mutex->event = os_event_create();
	mutex_set_waiters(mutex, 0);
#ifdef UNIV_DEBUG
	mutex->magic_n = MUTEX_MAGIC_N;
#endif /* UNIV_DEBUG */
#ifdef UNIV_SYNC_DEBUG
	mutex->line = 0;
	mutex->file_name = "not yet reserved";
	mutex->level = level;
#endif /* UNIV_SYNC_DEBUG */
	mutex->cfile_name = cfile_name;
	mutex->cline = cline;
	mutex->count_os_wait = 0;

	/* Check that lock_word is aligned; this is important on Intel */
	ut_ad(((ulint)(&(mutex->lock_word))) % 4 == 0);

	/* NOTE! The very first mutexes are not put to the mutex list */

	if ((mutex == &mutex_list_mutex)
#ifdef UNIV_SYNC_DEBUG
	    || (mutex == &sync_thread_mutex)
#endif /* UNIV_SYNC_DEBUG */
	    ) {

		return;
	}

	mutex_enter(&mutex_list_mutex);

	ut_ad(UT_LIST_GET_LEN(mutex_list) == 0
	      || UT_LIST_GET_FIRST(mutex_list)->magic_n == MUTEX_MAGIC_N);

	UT_LIST_ADD_FIRST(list, mutex_list, mutex);

	mutex_exit(&mutex_list_mutex);
}

/******************************************************************//**
NOTE! Use the corresponding macro mutex_free(), not directly this function!
Calling this function is obligatory only if the memory buffer containing
the mutex is freed. Removes a mutex object from the mutex list. The mutex
is checked to be in the reset state. */
UNIV_INTERN
void
mutex_free_func(
/*============*/
	ib_mutex_t*	mutex)	/*!< in: mutex */
{
	ut_ad(mutex_validate(mutex));
	ut_a(mutex_get_lock_word(mutex) == 0);
	ut_a(mutex_get_waiters(mutex) == 0);

#ifdef UNIV_MEM_DEBUG
	if (mutex == &mem_hash_mutex) {
		ut_ad(UT_LIST_GET_LEN(mutex_list) == 1);
		ut_ad(UT_LIST_GET_FIRST(mutex_list) == &mem_hash_mutex);
		UT_LIST_REMOVE(list, mutex_list, mutex);
		goto func_exit;
	}
#endif /* UNIV_MEM_DEBUG */

	if (mutex != &mutex_list_mutex
#ifdef UNIV_SYNC_DEBUG
	    && mutex != &sync_thread_mutex
#endif /* UNIV_SYNC_DEBUG */
	    ) {

		mutex_enter(&mutex_list_mutex);

		ut_ad(!UT_LIST_GET_PREV(list, mutex)
		      || UT_LIST_GET_PREV(list, mutex)->magic_n
		      == MUTEX_MAGIC_N);
		ut_ad(!UT_LIST_GET_NEXT(list, mutex)
		      || UT_LIST_GET_NEXT(list, mutex)->magic_n
		      == MUTEX_MAGIC_N);

		UT_LIST_REMOVE(list, mutex_list, mutex);

		mutex_exit(&mutex_list_mutex);
	}

	os_event_free(mutex->event);
#ifdef UNIV_MEM_DEBUG
func_exit:
#endif /* UNIV_MEM_DEBUG */
#if !defined(HAVE_ATOMIC_BUILTINS)
	os_fast_mutex_free(&(mutex->os_fast_mutex));
#endif
	/* If we free the mutex protecting the mutex list (freeing is
	not necessary), we have to reset the magic number AFTER removing
	it from the list. */
#ifdef UNIV_DEBUG
	mutex->magic_n = 0;
#endif /* UNIV_DEBUG */
	return;
}

/********************************************************************//**
NOTE! Use the corresponding macro in the header file, not this function
directly. Tries to lock the mutex for the current thread. If the lock is not
acquired immediately, returns with return value 1.
@return	0 if succeed, 1 if not */
UNIV_INTERN
ulint
mutex_enter_nowait_func(
/*====================*/
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name __attribute__((unused)),
					/*!< in: file name where mutex
					requested */
	ulint		line __attribute__((unused)))
					/*!< in: line where requested */
{
	ut_ad(mutex_validate(mutex));

	if (!ib_mutex_test_and_set(mutex)) {

		ut_d(mutex->thread_id = os_thread_get_curr_id());
#ifdef UNIV_SYNC_DEBUG
		mutex_set_debug_info(mutex, file_name, line);
#endif

		return(0);	/* Succeeded! */
	}

	return(1);
}

#ifdef UNIV_DEBUG
/******************************************************************//**
Checks that the mutex has been initialized.
@return	TRUE */
UNIV_INTERN
ibool
mutex_validate(
/*===========*/
	const ib_mutex_t*	mutex)	/*!< in: mutex */
{
	ut_a(mutex);
	ut_a(mutex->magic_n == MUTEX_MAGIC_N);

	return(TRUE);
}

/******************************************************************//**
Checks that the current thread owns the mutex. Works only in the debug
version.
@return	TRUE if owns */
UNIV_INTERN
ibool
mutex_own(
/*======*/
	const ib_mutex_t*	mutex)	/*!< in: mutex */
{
	ut_ad(mutex_validate(mutex));

	return(mutex_get_lock_word(mutex) == 1
	       && os_thread_eq(mutex->thread_id, os_thread_get_curr_id()));
}
#endif /* UNIV_DEBUG */

/******************************************************************//**
Sets the waiters field in a mutex. */
UNIV_INTERN
void
mutex_set_waiters(
/*==============*/
	ib_mutex_t*	mutex,	/*!< in: mutex */
	ulint		n)	/*!< in: value to set */
{
	volatile ulint*	ptr;		/* declared volatile to ensure that
					the value is stored to memory */
	ut_ad(mutex);

	ptr = &(mutex->waiters);

	*ptr = n;		/* Here we assume that the write of a single
				word in memory is atomic */
}

/******************************************************************//**
Reserves a mutex for the current thread. If the mutex is reserved, the
function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the mutex before suspending the thread. */
UNIV_INTERN
void
mutex_spin_wait(
/*============*/
	ib_mutex_t*	mutex,		/*!< in: pointer to mutex */
	const char*	file_name,	/*!< in: file name where mutex
					requested */
	ulint		line)		/*!< in: line where requested */
{
	ulint		i;		/* spin round count */
	ulint		index;		/* index of the reserved wait cell */
	sync_array_t*	sync_arr;
	size_t		counter_index;

	counter_index = (size_t) os_thread_get_curr_id();

	ut_ad(mutex);

	/* This update is not thread safe, but we don't mind if the count
	isn't exact. Moved out of ifdef that follows because we are willing
	to sacrifice the cost of counting this as the data is valuable.
	Count the number of calls to mutex_spin_wait. */
	mutex_spin_wait_count.add(counter_index, 1);

mutex_loop:

	i = 0;

	/* Spin waiting for the lock word to become zero. Note that we do
	not have to assume that the read access to the lock word is atomic,
	as the actual locking is always committed with atomic test-and-set.
	In reality, however, all processors probably have an atomic read of
	a memory word. */

spin_loop:

	while (mutex_get_lock_word(mutex) != 0 && i < SYNC_SPIN_ROUNDS) {
		if (srv_spin_wait_delay) {
			ut_delay(ut_rnd_interval(0, srv_spin_wait_delay));
		}

		i++;
	}

	if (i == SYNC_SPIN_ROUNDS) {
		os_thread_yield();
	}

	mutex_spin_round_count.add(counter_index, i);

	if (ib_mutex_test_and_set(mutex) == 0) {
		/* Succeeded! */

		ut_d(mutex->thread_id = os_thread_get_curr_id());
#ifdef UNIV_SYNC_DEBUG
		mutex_set_debug_info(mutex, file_name, line);
#endif
		return;
	}

	/* We may end up with a situation where lock_word is 0 but the OS
	fast mutex is still reserved. On FreeBSD the OS does not seem to
	schedule a thread which is constantly calling pthread_mutex_trylock
	(in ib_mutex_test_and_set implementation). Then we could end up
	spinning here indefinitely. The following 'i++' stops this infinite
	spin. */

	i++;

	if (i < SYNC_SPIN_ROUNDS) {
		goto spin_loop;
	}

	sync_arr = sync_array_get();

	sync_array_reserve_cell(
		sync_arr, mutex, SYNC_MUTEX, file_name, line, &index);

	/* The memory order of the array reservation and the change in the
	waiters field is important: when we suspend a thread, we first
	reserve the cell and then set waiters field to 1. When threads are
	released in mutex_exit, the waiters field is first set to zero and
	then the event is set to the signaled state. */

	mutex_set_waiters(mutex, 1);

	/* Try to reserve still a few times */
	for (i = 0; i < 4; i++) {
		if (ib_mutex_test_and_set(mutex) == 0) {
			/* Succeeded! Free the reserved wait cell */

			sync_array_free_cell(sync_arr, index);

			ut_d(mutex->thread_id = os_thread_get_curr_id());
#ifdef UNIV_SYNC_DEBUG
			mutex_set_debug_info(mutex, file_name, line);
#endif

			return;

			/* Note that in this case we leave the waiters field
			set to 1. We cannot reset it to zero, as we do not
			know if there are other waiters. */
		}
	}

	/* Now we know that there has been some thread holding the mutex
	after the change in the wait array and the waiters field was made.
	Now there is no risk of infinite wait on the event. */

	mutex_os_wait_count.add(counter_index, 1);

	mutex->count_os_wait++;

	sync_array_wait_event(sync_arr, index);

	goto mutex_loop;
}

/******************************************************************//**
Releases the threads waiting in the primary wait array for this mutex. */
UNIV_INTERN
void
mutex_signal_object(
/*================*/
	ib_mutex_t*	mutex)	/*!< in: mutex */
{
	mutex_set_waiters(mutex, 0);

	/* The memory order of resetting the waiters field and
	signaling the object is important. See LEMMA 1 above. */
	os_event_set(mutex->event);
	sync_array_object_signalled();
}

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Sets the debug information for a reserved mutex. */
UNIV_INTERN
void
mutex_set_debug_info(
/*=================*/
	ib_mutex_t*	mutex,		/*!< in: mutex */
	const char*	file_name,	/*!< in: file where requested */
	ulint		line)		/*!< in: line where requested */
{
	ut_ad(mutex);
	ut_ad(file_name);

	sync_thread_add_level(mutex, mutex->level, FALSE);

	mutex->file_name = file_name;
	mutex->line	 = line;
}

/******************************************************************//**
Gets the debug information for a reserved mutex. */
UNIV_INTERN
void
mutex_get_debug_info(
/*=================*/
	ib_mutex_t*	mutex,		/*!< in: mutex */
	const char**	file_name,	/*!< out: file where requested */
	ulint*		line,		/*!< out: line where requested */
	os_thread_id_t* thread_id)	/*!< out: id of the thread which owns
					the mutex */
{
	ut_ad(mutex);

	*file_name = mutex->file_name;
	*line	   = mutex->line;
	*thread_id = mutex->thread_id;
}

/******************************************************************//**
Prints debug info of currently reserved mutexes. */
static
void
mutex_list_print_info(
/*==================*/
	FILE*	file)		/*!< in: file where to print */
{
	ib_mutex_t*	mutex;
	const char*	file_name;
	ulint		line;
	os_thread_id_t	thread_id;
	ulint		count		= 0;

	fputs("----------\n"
	      "MUTEX INFO\n"
	      "----------\n", file);

	mutex_enter(&mutex_list_mutex);

	mutex = UT_LIST_GET_FIRST(mutex_list);

	while (mutex != NULL) {
		count++;

		if (mutex_get_lock_word(mutex) != 0) {
			mutex_get_debug_info(mutex, &file_name, &line,
					     &thread_id);
			fprintf(file,
				"Locked mutex: addr %p thread %ld"
				" file %s line %ld\n",
				(void*) mutex, os_thread_pf(thread_id),
				file_name, line);
		}

		mutex = UT_LIST_GET_NEXT(list, mutex);
	}

	fprintf(file, "Total number of mutexes %ld\n", count);

	mutex_exit(&mutex_list_mutex);
}

/******************************************************************//**
Counts currently reserved mutexes. Works only in the debug version.
@return	number of reserved mutexes */
UNIV_INTERN
ulint
mutex_n_reserved(void)
/*==================*/
{
	ib_mutex_t*	mutex;
	ulint		count	= 0;

	mutex_enter(&mutex_list_mutex);

	for (mutex = UT_LIST_GET_FIRST(mutex_list);
	     mutex != NULL;
	     mutex = UT_LIST_GET_NEXT(list, mutex)) {

		if (mutex_get_lock_word(mutex) != 0) {

			count++;
		}
	}

	mutex_exit(&mutex_list_mutex);

	ut_a(count >= 1);

	/* Subtract one, because this function itself was holding
	one mutex (mutex_list_mutex) */

	return(count - 1);
}

/******************************************************************//**
Returns TRUE if no mutex or rw-lock is currently locked. Works only in
the debug version.
@return	TRUE if no mutexes and rw-locks reserved */
UNIV_INTERN
ibool
sync_all_freed(void)
/*================*/
{
	return(mutex_n_reserved() + rw_lock_n_locked() == 0);
}

/******************************************************************//**
Looks for the thread slot for the calling thread.
@return	pointer to thread slot, NULL if not found */
static
sync_thread_t*
sync_thread_level_arrays_find_slot(void)
/*====================================*/

{
	ulint		i;
	os_thread_id_t	id;

	id = os_thread_get_curr_id();

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		sync_thread_t*	slot;

		slot = &sync_thread_level_arrays[i];

		if (slot->levels && os_thread_eq(slot->id, id)) {

			return(slot);
		}
	}

	return(NULL);
}

/******************************************************************//**
Looks for an unused thread slot.
@return	pointer to thread slot */
static
sync_thread_t*
sync_thread_level_arrays_find_free(void)
/*====================================*/

{
	ulint		i;

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		sync_thread_t*	slot;

		slot = &sync_thread_level_arrays[i];

		if (slot->levels == NULL) {

			return(slot);
		}
	}

	return(NULL);
}

/******************************************************************//**
Print warning. */
static
void
sync_print_warning(
/*===============*/
	const sync_level_t*	slot)	/*!< in: slot for which to
					print warning */
{
	ib_mutex_t*	mutex;

	mutex = static_cast<ib_mutex_t*>(slot->latch);

	if (mutex->magic_n == MUTEX_MAGIC_N) {
		fprintf(stderr,
			"Mutex created at %s %lu\n",
			innobase_basename(mutex->cfile_name),
			(ulong) mutex->cline);

		if (mutex_get_lock_word(mutex) != 0) {
			ulint		line;
			const char*	file_name;
			os_thread_id_t	thread_id;

			mutex_get_debug_info(
				mutex, &file_name, &line, &thread_id);

			fprintf(stderr,
				"InnoDB: Locked mutex:"
				" addr %p thread %ld file %s line %ld\n",
				(void*) mutex, os_thread_pf(thread_id),
				file_name, (ulong) line);
		} else {
			fputs("Not locked\n", stderr);
		}
	} else {
		rw_lock_t*	lock;

		lock = static_cast<rw_lock_t*>(slot->latch);

		rw_lock_print(lock);
	}
}

/******************************************************************//**
Checks if all the level values stored in the level array are greater than
the given limit.
@return	TRUE if all greater */
static
ibool
sync_thread_levels_g(
/*=================*/
	sync_arr_t*	arr,	/*!< in: pointer to level array for an OS
				thread */
	ulint		limit,	/*!< in: level limit */
	ulint		warn)	/*!< in: TRUE=display a diagnostic message */
{
	ulint		i;

	for (i = 0; i < arr->n_elems; i++) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL && slot->level <= limit) {
			if (warn) {
				fprintf(stderr,
					"InnoDB: sync levels should be"
					" > %lu but a level is %lu\n",
					(ulong) limit, (ulong) slot->level);

				sync_print_warning(slot);
			}

			return(FALSE);
		}
	}

	return(TRUE);
}

/******************************************************************//**
Checks if the level value is stored in the level array.
@return	slot if found or NULL */
static
const sync_level_t*
sync_thread_levels_contain(
/*=======================*/
	sync_arr_t*	arr,	/*!< in: pointer to level array for an OS
				thread */
	ulint		level)	/*!< in: level */
{
	ulint		i;

	for (i = 0; i < arr->n_elems; i++) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL && slot->level == level) {

			return(slot);
		}
	}

	return(NULL);
}

/******************************************************************//**
Checks if the level value and latch is already stored in the level array.
@return	slot if found or NULL */
static
sync_level_t*
sync_thread_levels_find(
/*====================*/
	sync_arr_t*	arr,	/*!< in: pointer to level array for an OS
				thread */
	void*		latch,	/*!< in: pointer to a mutex or an rw-lock */
	ulint		level)	/*!< in: level */
{
	for (ulint i = 0; i < arr->n_elems; i++) {
		sync_level_t*	slot = &arr->elems[i];

		if (slot->latch == latch && slot->level == level) {
			return(slot);
		}
	}

	return(NULL);
}

/******************************************************************//**
Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@return	a matching latch, or NULL if not found */
UNIV_INTERN
void*
sync_thread_levels_contains(
/*========================*/
	ulint	level)			/*!< in: latching order level
					(SYNC_DICT, ...)*/
{
	ulint		i;
	sync_arr_t*	arr;
	sync_thread_t*	thread_slot;

	if (!sync_order_checks_on) {

		return(NULL);
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {

		mutex_exit(&sync_thread_mutex);

		return(NULL);
	}

	arr = thread_slot->levels;

	for (i = 0; i < arr->n_elems; i++) {
		sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL && slot->level == level) {

			mutex_exit(&sync_thread_mutex);
			return(slot->latch);
		}
	}

	mutex_exit(&sync_thread_mutex);

	return(NULL);
}

/******************************************************************//**
Checks that the level array for the current thread is empty.
@return	a latch, or NULL if empty except the exceptions specified below */
UNIV_INTERN
void*
sync_thread_levels_nonempty_gen(
/*============================*/
	ibool	dict_mutex_allowed)	/*!< in: TRUE if dictionary mutex is
					allowed to be owned by the thread */
{
	ulint		i;
	sync_arr_t*	arr;
	sync_thread_t*	thread_slot;

	if (!sync_order_checks_on) {

		return(NULL);
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {

		mutex_exit(&sync_thread_mutex);

		return(NULL);
	}

	arr = thread_slot->levels;

	for (i = 0; i < arr->n_elems; ++i) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL
		    && (!dict_mutex_allowed
			|| (slot->level != SYNC_DICT
			    && slot->level != SYNC_DICT_OPERATION
			    && slot->level != SYNC_FTS_CACHE))) {

			mutex_exit(&sync_thread_mutex);
			ut_error;

			return(slot->latch);
		}
	}

	mutex_exit(&sync_thread_mutex);

	return(NULL);
}

/******************************************************************//**
Checks if the level array for the current thread is empty,
except for the btr_search_latch.
@return	a latch, or NULL if empty except the exceptions specified below */
UNIV_INTERN
void*
sync_thread_levels_nonempty_trx(
/*============================*/
	ibool	has_search_latch)
				/*!< in: TRUE if and only if the thread
				is supposed to hold btr_search_latch */
{
	ulint		i;
	sync_arr_t*	arr;
	sync_thread_t*	thread_slot;

	if (!sync_order_checks_on) {

		return(NULL);
	}

	ut_a(!has_search_latch
	     || sync_thread_levels_contains(SYNC_SEARCH_SYS));

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {

		mutex_exit(&sync_thread_mutex);

		return(NULL);
	}

	arr = thread_slot->levels;

	for (i = 0; i < arr->n_elems; ++i) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL
		    && (!has_search_latch
			|| slot->level != SYNC_SEARCH_SYS)) {

			mutex_exit(&sync_thread_mutex);
			ut_error;

			return(slot->latch);
		}
	}

	mutex_exit(&sync_thread_mutex);

	return(NULL);
}

/******************************************************************//**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */
UNIV_INTERN
void
sync_thread_add_level(
/*==================*/
	void*	latch,	/*!< in: pointer to a mutex or an rw-lock */
	ulint	level,	/*!< in: level in the latching order; if
			SYNC_LEVEL_VARYING, nothing is done */
	ibool	relock)	/*!< in: TRUE if re-entering an x-lock */
{
	ulint		i;
	sync_level_t*	slot;
	sync_arr_t*	array;
	sync_thread_t*	thread_slot;

	if (!sync_order_checks_on) {

		return;
	}

	if ((latch == (void*) &sync_thread_mutex)
	    || (latch == (void*) &mutex_list_mutex)
	    || (latch == (void*) &rw_lock_debug_mutex)
	    || (latch == (void*) &rw_lock_list_mutex)) {

		return;
	}

	if (level == SYNC_LEVEL_VARYING) {

		return;
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {
		ulint	sz;

		sz = sizeof(*array)
		   + (sizeof(*array->elems) * SYNC_THREAD_N_LEVELS);

		/* We have to allocate the level array for a new thread */
		array = static_cast<sync_arr_t*>(calloc(sz, sizeof(char)));
		ut_a(array != NULL);

		array->next_free = ULINT_UNDEFINED;
		array->max_elems = SYNC_THREAD_N_LEVELS;
		array->elems = (sync_level_t*) &array[1];

		thread_slot = sync_thread_level_arrays_find_free();

		thread_slot->levels = array;
		thread_slot->id = os_thread_get_curr_id();
	}

	array = thread_slot->levels;

	if (relock) {
		goto levels_ok;
	}

	/* NOTE that there is a problem with _NODE and _LEAF levels: if the
	B-tree height changes, then a leaf can change to an internal node
	or the other way around. We do not know at present if this can cause
	unnecessary assertion failures below. */

	switch (level) {
	case SYNC_NO_ORDER_CHECK:
	case SYNC_EXTERN_STORAGE:
	case SYNC_TREE_NODE_FROM_HASH:
		/* Do no order checking */
		break;
	case SYNC_TRX_SYS_HEADER:
		if (srv_is_being_started) {
			/* This is violated during trx_sys_create_rsegs()
			when creating additional rollback segments when
			upgrading in innobase_start_or_create_for_mysql(). */
			break;
		}
	case SYNC_MEM_POOL:
	case SYNC_MEM_HASH:
	case SYNC_RECV:
	case SYNC_FTS_BG_THREADS:
	case SYNC_WORK_QUEUE:
	case SYNC_FTS_OPTIMIZE:
	case SYNC_FTS_CACHE:
	case SYNC_FTS_CACHE_INIT:
	case SYNC_LOG:
	case SYNC_LOG_FLUSH_ORDER:
	case SYNC_ANY_LATCH:
	case SYNC_FILE_FORMAT_TAG:
	case SYNC_DOUBLEWRITE:
	case SYNC_SEARCH_SYS:
	case SYNC_THREADS:
	case SYNC_LOCK_SYS:
	case SYNC_LOCK_WAIT_SYS:
	case SYNC_TRX_SYS:
	case SYNC_IBUF_BITMAP_MUTEX:
	case SYNC_RSEG:
	case SYNC_TRX_UNDO:
	case SYNC_PURGE_LATCH:
	case SYNC_PURGE_QUEUE:
	case SYNC_DICT_AUTOINC_MUTEX:
	case SYNC_DICT_OPERATION:
	case SYNC_DICT_HEADER:
	case SYNC_TRX_I_S_RWLOCK:
	case SYNC_TRX_I_S_LAST_READ:
	case SYNC_IBUF_MUTEX:
	case SYNC_INDEX_ONLINE_LOG:
	case SYNC_STATS_AUTO_RECALC:
		if (!sync_thread_levels_g(array, level, TRUE)) {
			fprintf(stderr,
				"InnoDB: sync_thread_levels_g(array, %lu)"
				" does not hold!\n", level);
			ut_error;
		}
		break;
	case SYNC_TRX:
		/* Either the thread must own the lock_sys->mutex, or
		it is allowed to own only ONE trx->mutex. */
		if (!sync_thread_levels_g(array, level, FALSE)) {
			ut_a(sync_thread_levels_g(array, level - 1, TRUE));
			ut_a(sync_thread_levels_contain(array, SYNC_LOCK_SYS));
		}
		break;
	case SYNC_BUF_FLUSH_LIST:
	case SYNC_BUF_POOL:
		/* We can have multiple mutexes of this type therefore we
		can only check whether the greater than condition holds. */
		if (!sync_thread_levels_g(array, level-1, TRUE)) {
			fprintf(stderr,
				"InnoDB: sync_thread_levels_g(array, %lu)"
				" does not hold!\n", level-1);
			ut_error;
		}
		break;


	case SYNC_BUF_PAGE_HASH:
		/* Multiple page_hash locks are only allowed during
		buf_validate and that is where buf_pool mutex is already
		held. */
		/* Fall through */

	case SYNC_BUF_BLOCK:
		/* Either the thread must own the buffer pool mutex
		(buf_pool->mutex), or it is allowed to latch only ONE
		buffer block (block->mutex or buf_pool->zip_mutex). */
		if (!sync_thread_levels_g(array, level, FALSE)) {
			ut_a(sync_thread_levels_g(array, level - 1, TRUE));
			ut_a(sync_thread_levels_contain(array, SYNC_BUF_POOL));
		}
		break;
	case SYNC_REC_LOCK:
		if (sync_thread_levels_contain(array, SYNC_LOCK_SYS)) {
			ut_a(sync_thread_levels_g(array, SYNC_REC_LOCK - 1,
						  TRUE));
		} else {
			ut_a(sync_thread_levels_g(array, SYNC_REC_LOCK, TRUE));
		}
		break;
	case SYNC_IBUF_BITMAP:
		/* Either the thread must own the master mutex to all
		the bitmap pages, or it is allowed to latch only ONE
		bitmap page. */
		if (sync_thread_levels_contain(array,
					       SYNC_IBUF_BITMAP_MUTEX)) {
			ut_a(sync_thread_levels_g(array, SYNC_IBUF_BITMAP - 1,
						  TRUE));
		} else {
			/* This is violated during trx_sys_create_rsegs()
			when creating additional rollback segments when
			upgrading in innobase_start_or_create_for_mysql(). */
			ut_a(srv_is_being_started
			     || sync_thread_levels_g(array, SYNC_IBUF_BITMAP,
						     TRUE));
		}
		break;
	case SYNC_FSP_PAGE:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP));
		break;
	case SYNC_FSP:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP)
		     || sync_thread_levels_g(array, SYNC_FSP, TRUE));
		break;
	case SYNC_TRX_UNDO_PAGE:
		/* Purge is allowed to read in as many UNDO pages as it likes,
		there was a bogus rule here earlier that forced the caller to
		acquire the purge_sys_t::mutex. The purge mutex did not really
		protect anything because it was only ever acquired by the
		single purge thread. The purge thread can read the UNDO pages
		without any covering mutex. */

		ut_a(sync_thread_levels_contain(array, SYNC_TRX_UNDO)
		     || sync_thread_levels_contain(array, SYNC_RSEG)
		     || sync_thread_levels_g(array, level - 1, TRUE));
		break;
	case SYNC_RSEG_HEADER:
		ut_a(sync_thread_levels_contain(array, SYNC_RSEG));
		break;
	case SYNC_RSEG_HEADER_NEW:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP_PAGE));
		break;
	case SYNC_TREE_NODE:
		ut_a(sync_thread_levels_contain(array, SYNC_INDEX_TREE)
		     || sync_thread_levels_contain(array, SYNC_DICT_OPERATION)
		     || sync_thread_levels_g(array, SYNC_TREE_NODE - 1, TRUE));
		break;
	case SYNC_TREE_NODE_NEW:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP_PAGE));
		break;
	case SYNC_INDEX_TREE:
		ut_a(sync_thread_levels_g(array, SYNC_TREE_NODE - 1, TRUE));
		break;
	case SYNC_IBUF_TREE_NODE:
		ut_a(sync_thread_levels_contain(array, SYNC_IBUF_INDEX_TREE)
		     || sync_thread_levels_g(array, SYNC_IBUF_TREE_NODE - 1,
					     TRUE));
		break;
	case SYNC_IBUF_TREE_NODE_NEW:
		/* ibuf_add_free_page() allocates new pages for the
		change buffer while only holding the tablespace
		x-latch. These pre-allocated new pages may only be
		taken in use while holding ibuf_mutex, in
		btr_page_alloc_for_ibuf(). */
		ut_a(sync_thread_levels_contain(array, SYNC_IBUF_MUTEX)
		     || sync_thread_levels_contain(array, SYNC_FSP));
		break;
	case SYNC_IBUF_INDEX_TREE:
		if (sync_thread_levels_contain(array, SYNC_FSP)) {
			ut_a(sync_thread_levels_g(array, level - 1, TRUE));
		} else {
			ut_a(sync_thread_levels_g(
				     array, SYNC_IBUF_TREE_NODE - 1, TRUE));
		}
		break;
	case SYNC_IBUF_PESS_INSERT_MUTEX:
		ut_a(sync_thread_levels_g(array, SYNC_FSP - 1, TRUE));
		ut_a(!sync_thread_levels_contain(array, SYNC_IBUF_MUTEX));
		break;
	case SYNC_IBUF_HEADER:
		ut_a(sync_thread_levels_g(array, SYNC_FSP - 1, TRUE));
		ut_a(!sync_thread_levels_contain(array, SYNC_IBUF_MUTEX));
		ut_a(!sync_thread_levels_contain(array,
						 SYNC_IBUF_PESS_INSERT_MUTEX));
		break;
	case SYNC_DICT:
#ifdef UNIV_DEBUG
		ut_a(buf_debug_prints
		     || sync_thread_levels_g(array, SYNC_DICT, TRUE));
#else /* UNIV_DEBUG */
		ut_a(sync_thread_levels_g(array, SYNC_DICT, TRUE));
#endif /* UNIV_DEBUG */
		break;
	default:
		ut_error;
	}

levels_ok:
	/* Look for this latch and level in the active list. If it is
	already there, then this is a recursive lock. */
	slot = sync_thread_levels_find(array, latch, level);
	if (slot != NULL) {
		slot->count++;
		mutex_exit(&sync_thread_mutex);
		return;
	}

	/* Get a free slot to track this level and latch */
	if (array->next_free == ULINT_UNDEFINED) {
		ut_a(array->n_elems < array->max_elems);

		i = array->n_elems++;
	} else {
		i = array->next_free;
		array->next_free = array->elems[i].level;
	}

	ut_a(i < array->n_elems);
	ut_a(i != ULINT_UNDEFINED);

	++array->in_use;

	slot = &array->elems[i];

	ut_a(slot->latch == NULL);

	slot->latch = latch;
	slot->level = level;
	slot->count = 1;

	mutex_exit(&sync_thread_mutex);
}

/******************************************************************//**
Removes a latch from the thread level array if it is found there.
@return TRUE if found in the array; it is no error if the latch is
not found, as we presently are not able to determine the level for
every latch reservation the program does */
UNIV_INTERN
ibool
sync_thread_reset_level(
/*====================*/
	void*	latch)	/*!< in: pointer to a mutex or an rw-lock */
{
	sync_arr_t*	array;
	sync_thread_t*	thread_slot;
	ulint		i;

	if (!sync_order_checks_on) {

		return(FALSE);
	}

	if ((latch == (void*) &sync_thread_mutex)
	    || (latch == (void*) &mutex_list_mutex)
	    || (latch == (void*) &rw_lock_debug_mutex)
	    || (latch == (void*) &rw_lock_list_mutex)) {

		return(FALSE);
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {

		ut_error;

		mutex_exit(&sync_thread_mutex);
		return(FALSE);
	}

	array = thread_slot->levels;

	for (i = 0; i < array->n_elems; i++) {
		sync_level_t*	slot;

		slot = &array->elems[i];

		if (slot->latch != latch) {
			continue;
		}

		if (slot->count > 1) {
			/* Found a latch recursively locked */
			slot->count--;

			mutex_exit(&sync_thread_mutex);

			return(TRUE);
		}

		slot->latch = NULL;

		/* Update the free slot list. See comment in sync_level_t
		for the level field. */
		slot->level = array->next_free;
		array->next_free = i;

		ut_a(array->in_use >= 1);
		--array->in_use;

		/* If all cells are idle then reset the free
		list. The assumption is that this will save
		time when we need to scan up to n_elems. */

		if (array->in_use == 0) {
			array->n_elems = 0;
			array->next_free = ULINT_UNDEFINED;
		}

		mutex_exit(&sync_thread_mutex);

		return(TRUE);
	}

	if (((ib_mutex_t*) latch)->magic_n != MUTEX_MAGIC_N) {
		rw_lock_t*	rw_lock;

		rw_lock = (rw_lock_t*) latch;

		if (rw_lock->level == SYNC_LEVEL_VARYING) {
			mutex_exit(&sync_thread_mutex);

			return(TRUE);
		}
	}

	ut_error;

	mutex_exit(&sync_thread_mutex);

	return(FALSE);
}
#endif /* UNIV_SYNC_DEBUG */

/******************************************************************//**
Initializes the synchronization data structures. */
UNIV_INTERN
void
sync_init(void)
/*===========*/
{
	ut_a(sync_initialized == FALSE);

	sync_initialized = TRUE;

	sync_array_init(OS_THREAD_MAX_N);

#ifdef UNIV_SYNC_DEBUG
	/* Create the thread latch level array where the latch levels
	are stored for each OS thread */

	sync_thread_level_arrays = static_cast<sync_thread_t*>(
		calloc(sizeof(sync_thread_t), OS_THREAD_MAX_N));

	ut_a(sync_thread_level_arrays != NULL);

#endif /* UNIV_SYNC_DEBUG */
	/* Init the mutex list and create the mutex to protect it. */

	UT_LIST_INIT(mutex_list);
	mutex_create(mutex_list_mutex_key, &mutex_list_mutex,
		     SYNC_NO_ORDER_CHECK);
#ifdef UNIV_SYNC_DEBUG
	mutex_create(sync_thread_mutex_key, &sync_thread_mutex,
		     SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

	/* Init the rw-lock list and create the mutex to protect it. */

	UT_LIST_INIT(rw_lock_list);
	mutex_create(rw_lock_list_mutex_key, &rw_lock_list_mutex,
		     SYNC_NO_ORDER_CHECK);

#ifdef UNIV_SYNC_DEBUG
	mutex_create(rw_lock_debug_mutex_key, &rw_lock_debug_mutex,
		     SYNC_NO_ORDER_CHECK);

	rw_lock_debug_event = os_event_create();
	rw_lock_debug_waiters = FALSE;
#endif /* UNIV_SYNC_DEBUG */
}

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Frees all debug memory. */
static
void
sync_thread_level_arrays_free(void)
/*===============================*/

{
	ulint	i;

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		sync_thread_t*	slot;

		slot = &sync_thread_level_arrays[i];

		/* If this slot was allocated then free the slot memory too. */
		if (slot->levels != NULL) {
			free(slot->levels);
			slot->levels = NULL;
		}
	}

	free(sync_thread_level_arrays);
	sync_thread_level_arrays = NULL;
}
#endif /* UNIV_SYNC_DEBUG */

/******************************************************************//**
Frees the resources in InnoDB's own synchronization data structures. Use
os_sync_free() after calling this. */
UNIV_INTERN
void
sync_close(void)
/*===========*/
{
	ib_mutex_t*	mutex;

	sync_array_close();

	for (mutex = UT_LIST_GET_FIRST(mutex_list);
	     mutex != NULL;
	     /* No op */) {

#ifdef UNIV_MEM_DEBUG
		if (mutex == &mem_hash_mutex) {
			mutex = UT_LIST_GET_NEXT(list, mutex);
			continue;
		}
#endif /* UNIV_MEM_DEBUG */

		mutex_free(mutex);

		mutex = UT_LIST_GET_FIRST(mutex_list);
	}

	mutex_free(&mutex_list_mutex);
#ifdef UNIV_SYNC_DEBUG
	mutex_free(&sync_thread_mutex);

	/* Switch latching order checks on in sync0sync.cc */
	sync_order_checks_on = FALSE;

	sync_thread_level_arrays_free();
#endif /* UNIV_SYNC_DEBUG */

	sync_initialized = FALSE;
}

/*******************************************************************//**
Prints wait info of the sync system. */
UNIV_INTERN
void
sync_print_wait_info(
/*=================*/
	FILE*	file)		/*!< in: file where to print */
{
	fprintf(file,
		"Mutex spin waits "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n"
		"RW-shared spins "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n"
		"RW-excl spins "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n",
		(ib_uint64_t) mutex_spin_wait_count,
		(ib_uint64_t) mutex_spin_round_count,
		(ib_uint64_t) mutex_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_s_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_x_os_wait_count);

	fprintf(file,
		"Spin rounds per wait: %.2f mutex, %.2f RW-shared, "
		"%.2f RW-excl\n",
		(double) mutex_spin_round_count /
		(mutex_spin_wait_count ? mutex_spin_wait_count : 1),
		(double) rw_lock_stats.rw_s_spin_round_count /
		(rw_lock_stats.rw_s_spin_wait_count
		 ? rw_lock_stats.rw_s_spin_wait_count : 1),
		(double) rw_lock_stats.rw_x_spin_round_count /
		(rw_lock_stats.rw_x_spin_wait_count
		 ? rw_lock_stats.rw_x_spin_wait_count : 1));
}

/*******************************************************************//**
Prints info of the sync system. */
UNIV_INTERN
void
sync_print(
/*=======*/
	FILE*	file)		/*!< in: file where to print */
{
#ifdef UNIV_SYNC_DEBUG
	mutex_list_print_info(file);

	rw_lock_list_print_info(file);
#endif /* UNIV_SYNC_DEBUG */

	sync_array_print(file);

	sync_print_wait_info(file);
}
