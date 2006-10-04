/******************************************************
Mutex, the basic synchronization primitive

(c) 1995 Innobase Oy

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

LEMMA 1: After a thread resets the event of the cell it reserves for waiting
========
for a mutex, some thread will eventually call sync_array_signal_object with
the mutex as an argument. Thus no infinite wait is possible.

Proof:	After making the reservation the thread sets the waiters field in the
mutex to 1. Then it checks that the mutex is still reserved by some thread,
or it reserves the mutex for itself. In any case, some thread (which may be
also some earlier thread, not necessarily the one currently holding the mutex)
will set the waiters field to 0 in mutex_exit, and then call
sync_array_signal_object with the mutex as an argument.
Q.E.D. */

ulint	sync_dummy			= 0;

/* The number of system calls made in this module. Intended for performance
monitoring. */

ulint	mutex_system_call_count		= 0;

/* Number of spin waits on mutexes: for performance monitoring */

ulint	mutex_spin_round_count		= 0;
ulint	mutex_spin_wait_count		= 0;
ulint	mutex_os_wait_count		= 0;
ulint	mutex_exit_count		= 0;

/* The global array of wait cells for implementation of the database's own
mutexes and read-write locks */
sync_array_t*	sync_primary_wait_array;

/* This variable is set to TRUE when sync_init is called */
ibool	sync_initialized	= FALSE;


typedef struct sync_level_struct	sync_level_t;
typedef struct sync_thread_struct	sync_thread_t;

/* The latch levels currently owned by threads are stored in this data
structure; the size of this array is OS_THREAD_MAX_N */

sync_thread_t*	sync_thread_level_arrays;

/* Mutex protecting sync_thread_level_arrays */
mutex_t	sync_thread_mutex;

/* Global list of database mutexes (not OS mutexes) created. */
ut_list_base_node_t  mutex_list;

/* Mutex protecting the mutex_list variable */
mutex_t mutex_list_mutex;

/* Latching order checks start when this is set TRUE */
ibool	sync_order_checks_on	= FALSE;

/* Dummy mutex used to implement mutex_fence */
mutex_t	dummy_mutex_for_fence;

struct sync_thread_struct{
	os_thread_id_t	id;	/* OS thread id */
	sync_level_t*	levels;	/* level array for this thread; if this is NULL
				this slot is unused */
};

/* Number of slots reserved for each OS thread in the sync level array */
#define SYNC_THREAD_N_LEVELS	10000

struct sync_level_struct{
	void*	latch;	/* pointer to a mutex or an rw-lock; NULL means that
			the slot is empty */
	ulint	level;	/* level of the latch in the latching order */
};

/**********************************************************************
A noninlined function that reserves a mutex. In ha_innodb.cc we have disabled
inlining of InnoDB functions, and no inlined functions should be called from
there. That is why we need to duplicate the inlined function here. */

void
mutex_enter_noninline(
/*==================*/
	mutex_t*	mutex)	/* in: mutex */
{
	mutex_enter(mutex);
}

/**********************************************************************
Releases a mutex. */

void
mutex_exit_noninline(
/*=================*/
	mutex_t*	mutex)	/* in: mutex */
{
	mutex_exit(mutex);
}

/**********************************************************************
Creates, or rather, initializes a mutex object in a specified memory
location (which must be appropriately aligned). The mutex is initialized
in the reset state. Explicit freeing of the mutex with mutex_free is
necessary only if the memory block containing it is freed. */

void
mutex_create_func(
/*==============*/
	mutex_t*	mutex,		/* in: pointer to memory */
	ulint		level,		/* in: level */
	const char*	cfile_name,	/* in: file name where created */
  ulint cline,	/* in: file line where created */
  const char* cmutex_name)  /* in: mutex name */
{
#if defined(_WIN32) && defined(UNIV_CAN_USE_X86_ASSEMBLER)
	mutex_reset_lock_word(mutex);
#else
	os_fast_mutex_init(&(mutex->os_fast_mutex));
	mutex->lock_word = 0;
#endif
	mutex_set_waiters(mutex, 0);
	mutex->magic_n = MUTEX_MAGIC_N;
#ifdef UNIV_SYNC_DEBUG
	mutex->line = 0;
	mutex->file_name = "not yet reserved";
#endif /* UNIV_SYNC_DEBUG */
	mutex->level = level;
	mutex->cfile_name = cfile_name;
	mutex->cline = cline;
#ifndef UNIV_HOTBACKUP
	mutex->cmutex_name=	  cmutex_name;
	mutex->count_using=	  0;
	mutex->mutex_type=	  0;
	mutex->lspent_time=	  0;
	mutex->lmax_spent_time=     0;
	mutex->count_spin_loop= 0;
	mutex->count_spin_rounds=   0;
	mutex->count_os_wait=	  0;
	mutex->count_os_yield=  0;
#endif /* !UNIV_HOTBACKUP */

	/* Check that lock_word is aligned; this is important on Intel */
	ut_ad(((ulint)(&(mutex->lock_word))) % 4 == 0);

	/* NOTE! The very first mutexes are not put to the mutex list */

	if ((mutex == &mutex_list_mutex) || (mutex == &sync_thread_mutex)) {

		return;
	}

	mutex_enter(&mutex_list_mutex);

	if (UT_LIST_GET_LEN(mutex_list) > 0) {
		ut_a(UT_LIST_GET_FIRST(mutex_list)->magic_n == MUTEX_MAGIC_N);
	}

	UT_LIST_ADD_FIRST(list, mutex_list, mutex);

	mutex_exit(&mutex_list_mutex);
}

/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the mutex is freed. Removes a mutex object from the mutex list. The mutex
is checked to be in the reset state. */

void
mutex_free(
/*=======*/
	mutex_t*	mutex)	/* in: mutex */
{
#ifdef UNIV_DEBUG
	ut_a(mutex_validate(mutex));
#endif /* UNIV_DEBUG */
	ut_a(mutex_get_lock_word(mutex) == 0);
	ut_a(mutex_get_waiters(mutex) == 0);

	if (mutex != &mutex_list_mutex && mutex != &sync_thread_mutex) {

		mutex_enter(&mutex_list_mutex);

		if (UT_LIST_GET_PREV(list, mutex)) {
			ut_a(UT_LIST_GET_PREV(list, mutex)->magic_n
			     == MUTEX_MAGIC_N);
		}
		if (UT_LIST_GET_NEXT(list, mutex)) {
			ut_a(UT_LIST_GET_NEXT(list, mutex)->magic_n
			     == MUTEX_MAGIC_N);
		}

		UT_LIST_REMOVE(list, mutex_list, mutex);

		mutex_exit(&mutex_list_mutex);
	}

#if !defined(_WIN32) || !defined(UNIV_CAN_USE_X86_ASSEMBLER)
	os_fast_mutex_free(&(mutex->os_fast_mutex));
#endif
	/* If we free the mutex protecting the mutex list (freeing is
	not necessary), we have to reset the magic number AFTER removing
	it from the list. */

	mutex->magic_n = 0;
}

/************************************************************************
Tries to lock the mutex for the current thread. If the lock is not acquired
immediately, returns with return value 1. */

ulint
mutex_enter_nowait(
/*===============*/
					/* out: 0 if succeed, 1 if not */
	mutex_t*	mutex,		/* in: pointer to mutex */
	const char*	file_name __attribute__((unused)),
					/* in: file name where mutex
					requested */
	ulint		line __attribute__((unused)))
					/* in: line where requested */
{
	ut_ad(mutex_validate(mutex));

	if (!mutex_test_and_set(mutex)) {

#ifdef UNIV_SYNC_DEBUG
		mutex_set_debug_info(mutex, file_name, line);
#endif

		return(0);	/* Succeeded! */
	}

	return(1);
}

/**********************************************************************
Checks that the mutex has been initialized. */

ibool
mutex_validate(
/*===========*/
	mutex_t*	mutex)
{
	ut_a(mutex);
	ut_a(mutex->magic_n == MUTEX_MAGIC_N);

	return(TRUE);
}

/**********************************************************************
Sets the waiters field in a mutex. */

void
mutex_set_waiters(
/*==============*/
	mutex_t*	mutex,	/* in: mutex */
	ulint		n)	/* in: value to set */
{
	volatile ulint*	ptr;		/* declared volatile to ensure that
					the value is stored to memory */
	ut_ad(mutex);

	ptr = &(mutex->waiters);

	*ptr = n;		/* Here we assume that the write of a single
				word in memory is atomic */
}

/**********************************************************************
Reserves a mutex for the current thread. If the mutex is reserved, the
function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the mutex before suspending the thread. */

void
mutex_spin_wait(
/*============*/
	mutex_t*	mutex,		/* in: pointer to mutex */
	const char*	file_name,	/* in: file name where mutex
					requested */
	ulint		line)		/* in: line where requested */
{
	ulint	   index; /* index of the reserved wait cell */
	ulint	   i;	  /* spin round count */
#ifndef UNIV_HOTBACKUP
	ib_longlong lstart_time = 0, lfinish_time; /* for timing os_wait */
	ulint ltime_diff;
	ulint sec;
	ulint ms;
	uint timer_started = 0;
#endif /* !UNIV_HOTBACKUP */
	ut_ad(mutex);

mutex_loop:

	i = 0;

	/* Spin waiting for the lock word to become zero. Note that we do
	not have to assume that the read access to the lock word is atomic,
	as the actual locking is always committed with atomic test-and-set.
	In reality, however, all processors probably have an atomic read of
	a memory word. */

spin_loop:
#ifndef UNIV_HOTBACKUP
	mutex_spin_wait_count++;
	mutex->count_spin_loop++;
#endif /* !UNIV_HOTBACKUP */

	while (mutex_get_lock_word(mutex) != 0 && i < SYNC_SPIN_ROUNDS) {
		if (srv_spin_wait_delay) {
			ut_delay(ut_rnd_interval(0, srv_spin_wait_delay));
		}

		i++;
	}

	if (i == SYNC_SPIN_ROUNDS) {
#ifndef UNIV_HOTBACKUP
		mutex->count_os_yield++;
		if (timed_mutexes == 1 && timer_started==0) {
			ut_usectime(&sec, &ms);
			lstart_time= (ib_longlong)sec * 1000000 + ms;
			timer_started = 1;
		}
#endif /* !UNIV_HOTBACKUP */
		os_thread_yield();
	}

#ifdef UNIV_SRV_PRINT_LATCH_WAITS
	fprintf(stderr,
		"Thread %lu spin wait mutex at %p"
		" cfile %s cline %lu rnds %lu\n",
		(ulong) os_thread_pf(os_thread_get_curr_id()), (void*) mutex,
		mutex->cfile_name, (ulong) mutex->cline, (ulong) i);
#endif

	mutex_spin_round_count += i;

#ifndef UNIV_HOTBACKUP
	mutex->count_spin_rounds += i;
#endif /* !UNIV_HOTBACKUP */

	if (mutex_test_and_set(mutex) == 0) {
		/* Succeeded! */

#ifdef UNIV_SYNC_DEBUG
		mutex_set_debug_info(mutex, file_name, line);
#endif

		goto finish_timing;
	}

	/* We may end up with a situation where lock_word is 0 but the OS
	fast mutex is still reserved. On FreeBSD the OS does not seem to
	schedule a thread which is constantly calling pthread_mutex_trylock
	(in mutex_test_and_set implementation). Then we could end up
	spinning here indefinitely. The following 'i++' stops this infinite
	spin. */

	i++;

	if (i < SYNC_SPIN_ROUNDS) {
		goto spin_loop;
	}

	sync_array_reserve_cell(sync_primary_wait_array, mutex,
				SYNC_MUTEX, file_name, line, &index);

	mutex_system_call_count++;

	/* The memory order of the array reservation and the change in the
	waiters field is important: when we suspend a thread, we first
	reserve the cell and then set waiters field to 1. When threads are
	released in mutex_exit, the waiters field is first set to zero and
	then the event is set to the signaled state. */

	mutex_set_waiters(mutex, 1);

	/* Try to reserve still a few times */
	for (i = 0; i < 4; i++) {
		if (mutex_test_and_set(mutex) == 0) {
			/* Succeeded! Free the reserved wait cell */

			sync_array_free_cell_protected(sync_primary_wait_array,
						       index);

#ifdef UNIV_SYNC_DEBUG
			mutex_set_debug_info(mutex, file_name, line);
#endif

#ifdef UNIV_SRV_PRINT_LATCH_WAITS
			fprintf(stderr, "Thread %lu spin wait succeeds at 2:"
				" mutex at %p\n",
				(ulong) os_thread_pf(os_thread_get_curr_id()),
				(void*) mutex);
#endif

			goto finish_timing;

			/* Note that in this case we leave the waiters field
			set to 1. We cannot reset it to zero, as we do not
			know if there are other waiters. */
		}
	}

	/* Now we know that there has been some thread holding the mutex
	after the change in the wait array and the waiters field was made.
	Now there is no risk of infinite wait on the event. */

#ifdef UNIV_SRV_PRINT_LATCH_WAITS
	fprintf(stderr,
		"Thread %lu OS wait mutex at %p cfile %s cline %lu rnds %lu\n",
		(ulong) os_thread_pf(os_thread_get_curr_id()), (void*) mutex,
		mutex->cfile_name, (ulong) mutex->cline, (ulong) i);
#endif

	mutex_system_call_count++;
	mutex_os_wait_count++;

#ifndef UNIV_HOTBACKUP
	mutex->count_os_wait++;
	/* !!!!! Sometimes os_wait can be called without os_thread_yield */

	if (timed_mutexes == 1 && timer_started==0) {
		ut_usectime(&sec, &ms);
		lstart_time= (ib_longlong)sec * 1000000 + ms;
		timer_started = 1;
	}
#endif /* !UNIV_HOTBACKUP */

	sync_array_wait_event(sync_primary_wait_array, index);
	goto mutex_loop;

finish_timing:
#ifndef UNIV_HOTBACKUP
	if (timed_mutexes == 1 && timer_started==1) {
		ut_usectime(&sec, &ms);
		lfinish_time= (ib_longlong)sec * 1000000 + ms;

		ltime_diff= (ulint) (lfinish_time - lstart_time);
		mutex->lspent_time += ltime_diff;

		if (mutex->lmax_spent_time < ltime_diff) {
			mutex->lmax_spent_time= ltime_diff;
		}
	}
#endif /* !UNIV_HOTBACKUP */
	return;
}

/**********************************************************************
Releases the threads waiting in the primary wait array for this mutex. */

void
mutex_signal_object(
/*================*/
	mutex_t*	mutex)	/* in: mutex */
{
	mutex_set_waiters(mutex, 0);

	/* The memory order of resetting the waiters field and
	signaling the object is important. See LEMMA 1 above. */

	sync_array_signal_object(sync_primary_wait_array, mutex);
}

#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Sets the debug information for a reserved mutex. */

void
mutex_set_debug_info(
/*=================*/
	mutex_t*	mutex,		/* in: mutex */
	const char*	file_name,	/* in: file where requested */
	ulint		line)		/* in: line where requested */
{
	ut_ad(mutex);
	ut_ad(file_name);

	sync_thread_add_level(mutex, mutex->level);

	mutex->file_name = file_name;
	mutex->line	 = line;
	mutex->thread_id = os_thread_get_curr_id();
}

/**********************************************************************
Gets the debug information for a reserved mutex. */

void
mutex_get_debug_info(
/*=================*/
	mutex_t*	mutex,		/* in: mutex */
	const char**	file_name,	/* out: file where requested */
	ulint*		line,		/* out: line where requested */
	os_thread_id_t* thread_id)	/* out: id of the thread which owns
					the mutex */
{
	ut_ad(mutex);

	*file_name = mutex->file_name;
	*line	   = mutex->line;
	*thread_id = mutex->thread_id;
}
#endif /* UNIV_SYNC_DEBUG */

#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Checks that the current thread owns the mutex. Works only in the debug
version. */

ibool
mutex_own(
/*======*/
				/* out: TRUE if owns */
	mutex_t*	mutex)	/* in: mutex */
{
	ut_a(mutex_validate(mutex));

	if (mutex_get_lock_word(mutex) != 1) {

		return(FALSE);
	}

	if (!os_thread_eq(mutex->thread_id, os_thread_get_curr_id())) {

		return(FALSE);
	}

	return(TRUE);
}

/**********************************************************************
Prints debug info of currently reserved mutexes. */

void
mutex_list_print_info(void)
/*=======================*/
{
	mutex_t*	mutex;
	const char*	file_name;
	ulint		line;
	os_thread_id_t	thread_id;
	ulint		count		= 0;

	fputs("----------\n"
	      "MUTEX INFO\n"
	      "----------\n", stderr);

	mutex_enter(&mutex_list_mutex);

	mutex = UT_LIST_GET_FIRST(mutex_list);

	while (mutex != NULL) {
		count++;

		if (mutex_get_lock_word(mutex) != 0) {
			mutex_get_debug_info(mutex, &file_name, &line,
					     &thread_id);
			fprintf(stderr,
				"Locked mutex: addr %p thread %ld"
				" file %s line %ld\n",
				(void*) mutex, os_thread_pf(thread_id),
				file_name, line);
		}

		mutex = UT_LIST_GET_NEXT(list, mutex);
	}

	fprintf(stderr, "Total number of mutexes %ld\n", count);

	mutex_exit(&mutex_list_mutex);
}

/**********************************************************************
Counts currently reserved mutexes. Works only in the debug version. */

ulint
mutex_n_reserved(void)
/*==================*/
{
	mutex_t*	mutex;
	ulint		count		= 0;

	mutex_enter(&mutex_list_mutex);

	mutex = UT_LIST_GET_FIRST(mutex_list);

	while (mutex != NULL) {
		if (mutex_get_lock_word(mutex) != 0) {

			count++;
		}

		mutex = UT_LIST_GET_NEXT(list, mutex);
	}

	mutex_exit(&mutex_list_mutex);

	ut_a(count >= 1);

	return(count - 1); /* Subtract one, because this function itself
			   was holding one mutex (mutex_list_mutex) */
}

/**********************************************************************
Returns TRUE if no mutex or rw-lock is currently locked. Works only in
the debug version. */

ibool
sync_all_freed(void)
/*================*/
{
	return(mutex_n_reserved() + rw_lock_n_locked() == 0);
}
#endif /* UNIV_SYNC_DEBUG */

/**********************************************************************
Gets the value in the nth slot in the thread level arrays. */
static
sync_thread_t*
sync_thread_level_arrays_get_nth(
/*=============================*/
			/* out: pointer to thread slot */
	ulint	n)	/* in: slot number */
{
	ut_ad(n < OS_THREAD_MAX_N);

	return(sync_thread_level_arrays + n);
}

/**********************************************************************
Looks for the thread slot for the calling thread. */
static
sync_thread_t*
sync_thread_level_arrays_find_slot(void)
/*====================================*/
			/* out: pointer to thread slot, NULL if not found */

{
	sync_thread_t*	slot;
	os_thread_id_t	id;
	ulint		i;

	id = os_thread_get_curr_id();

	for (i = 0; i < OS_THREAD_MAX_N; i++) {

		slot = sync_thread_level_arrays_get_nth(i);

		if (slot->levels && os_thread_eq(slot->id, id)) {

			return(slot);
		}
	}

	return(NULL);
}

/**********************************************************************
Looks for an unused thread slot. */
static
sync_thread_t*
sync_thread_level_arrays_find_free(void)
/*====================================*/
			/* out: pointer to thread slot */

{
	sync_thread_t*	slot;
	ulint		i;

	for (i = 0; i < OS_THREAD_MAX_N; i++) {

		slot = sync_thread_level_arrays_get_nth(i);

		if (slot->levels == NULL) {

			return(slot);
		}
	}

	return(NULL);
}

/**********************************************************************
Gets the value in the nth slot in the thread level array. */
static
sync_level_t*
sync_thread_levels_get_nth(
/*=======================*/
				/* out: pointer to level slot */
	sync_level_t*	arr,	/* in: pointer to level array for an OS
				thread */
	ulint		n)	/* in: slot number */
{
	ut_ad(n < SYNC_THREAD_N_LEVELS);

	return(arr + n);
}

/**********************************************************************
Checks if all the level values stored in the level array are greater than
the given limit. */
static
ibool
sync_thread_levels_g(
/*=================*/
				/* out: TRUE if all greater */
	sync_level_t*	arr,	/* in: pointer to level array for an OS
				thread */
	ulint		limit)	/* in: level limit */
{
	sync_level_t*	slot;
	rw_lock_t*	lock;
	mutex_t*	mutex;
	ulint		i;

	for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

		slot = sync_thread_levels_get_nth(arr, i);

		if (slot->latch != NULL) {
			if (slot->level <= limit) {

				lock = slot->latch;
				mutex = slot->latch;

				fprintf(stderr,
					"InnoDB error: sync levels should be"
					" > %lu but a level is %lu\n",
					(ulong) limit, (ulong) slot->level);

				if (mutex->magic_n == MUTEX_MAGIC_N) {
					fprintf(stderr,
						"Mutex created at %s %lu\n",
						mutex->cfile_name,
						(ulong) mutex->cline);

					if (mutex_get_lock_word(mutex) != 0) {
#ifdef UNIV_SYNC_DEBUG
						const char*	file_name;
						ulint		line;
						os_thread_id_t	thread_id;

						mutex_get_debug_info
							(mutex, &file_name,
							 &line, &thread_id);

						fprintf(stderr,
							"InnoDB: Locked mutex:"
							" addr %p thread %ld"
							" file %s line %ld\n",
							(void*) mutex,
							os_thread_pf
							(thread_id),
							file_name,
							(ulong) line);
#else /* UNIV_SYNC_DEBUG */
						fprintf(stderr,
							"InnoDB: Locked mutex:"
							" addr %p\n",
							(void*) mutex);
#endif /* UNIV_SYNC_DEBUG */
					} else {
						fputs("Not locked\n", stderr);
					}
				} else {
#ifdef UNIV_SYNC_DEBUG
					rw_lock_print(lock);
#endif /* UNIV_SYNC_DEBUG */
				}

				return(FALSE);
			}
		}
	}

	return(TRUE);
}

/**********************************************************************
Checks if the level value is stored in the level array. */
static
ibool
sync_thread_levels_contain(
/*=======================*/
				/* out: TRUE if stored */
	sync_level_t*	arr,	/* in: pointer to level array for an OS
				thread */
	ulint		level)	/* in: level */
{
	sync_level_t*	slot;
	ulint		i;

	for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

		slot = sync_thread_levels_get_nth(arr, i);

		if (slot->latch != NULL) {
			if (slot->level == level) {

				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/**********************************************************************
Checks that the level array for the current thread is empty. */

ibool
sync_thread_levels_empty_gen(
/*=========================*/
					/* out: TRUE if empty except the
					exceptions specified below */
	ibool	dict_mutex_allowed)	/* in: TRUE if dictionary mutex is
					allowed to be owned by the thread,
					also purge_is_running mutex is
					allowed */
{
	sync_level_t*	arr;
	sync_thread_t*	thread_slot;
	sync_level_t*	slot;
	ulint		i;

	if (!sync_order_checks_on) {

		return(TRUE);
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {

		mutex_exit(&sync_thread_mutex);

		return(TRUE);
	}

	arr = thread_slot->levels;

	for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

		slot = sync_thread_levels_get_nth(arr, i);

		if (slot->latch != NULL
		    && (!dict_mutex_allowed
			|| (slot->level != SYNC_DICT
			    && slot->level != SYNC_DICT_OPERATION))) {

			mutex_exit(&sync_thread_mutex);
			ut_error;

			return(FALSE);
		}
	}

	mutex_exit(&sync_thread_mutex);

	return(TRUE);
}

/**********************************************************************
Checks that the level array for the current thread is empty. */

ibool
sync_thread_levels_empty(void)
/*==========================*/
			/* out: TRUE if empty */
{
	return(sync_thread_levels_empty_gen(FALSE));
}

/**********************************************************************
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread. */

void
sync_thread_add_level(
/*==================*/
	void*	latch,	/* in: pointer to a mutex or an rw-lock */
	ulint	level)	/* in: level in the latching order; if
			SYNC_LEVEL_VARYING, nothing is done */
{
	sync_level_t*	array;
	sync_level_t*	slot;
	sync_thread_t*	thread_slot;
	ulint		i;

	if (!sync_order_checks_on) {

		return;
	}

	if ((latch == (void*)&sync_thread_mutex)
	    || (latch == (void*)&mutex_list_mutex)
#ifdef UNIV_SYNC_DEBUG
	    || (latch == (void*)&rw_lock_debug_mutex)
#endif /* UNIV_SYNC_DEBUG */
	    || (latch == (void*)&rw_lock_list_mutex)) {

		return;
	}

	if (level == SYNC_LEVEL_VARYING) {

		return;
	}

	mutex_enter(&sync_thread_mutex);

	thread_slot = sync_thread_level_arrays_find_slot();

	if (thread_slot == NULL) {
		/* We have to allocate the level array for a new thread */
		array = ut_malloc(sizeof(sync_level_t) * SYNC_THREAD_N_LEVELS);

		thread_slot = sync_thread_level_arrays_find_free();

		thread_slot->id = os_thread_get_curr_id();
		thread_slot->levels = array;

		for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

			slot = sync_thread_levels_get_nth(array, i);

			slot->latch = NULL;
		}
	}

	array = thread_slot->levels;

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
	case SYNC_MEM_POOL:
		ut_a(sync_thread_levels_g(array, SYNC_MEM_POOL));
		break;
	case SYNC_MEM_HASH:
		ut_a(sync_thread_levels_g(array, SYNC_MEM_HASH));
		break;
	case SYNC_RECV:
		ut_a(sync_thread_levels_g(array, SYNC_RECV));
		break;
	case SYNC_WORK_QUEUE:
		ut_a(sync_thread_levels_g(array, SYNC_WORK_QUEUE));
		break;
	case SYNC_LOG:
		ut_a(sync_thread_levels_g(array, SYNC_LOG));
		break;
	case SYNC_THR_LOCAL:
		ut_a(sync_thread_levels_g(array, SYNC_THR_LOCAL));
		break;
	case SYNC_ANY_LATCH:
		ut_a(sync_thread_levels_g(array, SYNC_ANY_LATCH));
		break;
	case SYNC_TRX_SYS_HEADER:
		ut_a(sync_thread_levels_g(array, SYNC_TRX_SYS_HEADER));
		break;
	case SYNC_DOUBLEWRITE:
		ut_a(sync_thread_levels_g(array, SYNC_DOUBLEWRITE));
		break;
	case SYNC_BUF_BLOCK:
		ut_a((sync_thread_levels_contain(array, SYNC_BUF_POOL)
		      && sync_thread_levels_g(array, SYNC_BUF_BLOCK - 1))
		     || sync_thread_levels_g(array, SYNC_BUF_BLOCK));
		break;
	case SYNC_BUF_POOL:
		ut_a(sync_thread_levels_g(array, SYNC_BUF_POOL));
		break;
	case SYNC_SEARCH_SYS:
		ut_a(sync_thread_levels_g(array, SYNC_SEARCH_SYS));
		break;
	case SYNC_TRX_LOCK_HEAP:
		ut_a(sync_thread_levels_g(array, SYNC_TRX_LOCK_HEAP));
		break;
	case SYNC_REC_LOCK:
		ut_a((sync_thread_levels_contain(array, SYNC_KERNEL)
		      && sync_thread_levels_g(array, SYNC_REC_LOCK - 1))
		     || sync_thread_levels_g(array, SYNC_REC_LOCK));
		break;
	case SYNC_KERNEL:
		ut_a(sync_thread_levels_g(array, SYNC_KERNEL));
		break;
	case SYNC_IBUF_BITMAP:
		ut_a((sync_thread_levels_contain(array, SYNC_IBUF_BITMAP_MUTEX)
		      && sync_thread_levels_g(array, SYNC_IBUF_BITMAP - 1))
		     || sync_thread_levels_g(array, SYNC_IBUF_BITMAP));
		break;
	case SYNC_IBUF_BITMAP_MUTEX:
		ut_a(sync_thread_levels_g(array, SYNC_IBUF_BITMAP_MUTEX));
		break;
	case SYNC_FSP_PAGE:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP));
		break;
	case SYNC_FSP:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP)
		     || sync_thread_levels_g(array, SYNC_FSP));
		break;
	case SYNC_TRX_UNDO_PAGE:
		ut_a(sync_thread_levels_contain(array, SYNC_TRX_UNDO)
		     || sync_thread_levels_contain(array, SYNC_RSEG)
		     || sync_thread_levels_contain(array, SYNC_PURGE_SYS)
		     || sync_thread_levels_g(array, SYNC_TRX_UNDO_PAGE));
		break;
	case SYNC_RSEG_HEADER:
		ut_a(sync_thread_levels_contain(array, SYNC_RSEG));
		break;
	case SYNC_RSEG_HEADER_NEW:
		ut_a(sync_thread_levels_contain(array, SYNC_KERNEL)
		     && sync_thread_levels_contain(array, SYNC_FSP_PAGE));
		break;
	case SYNC_RSEG:
		ut_a(sync_thread_levels_g(array, SYNC_RSEG));
		break;
	case SYNC_TRX_UNDO:
		ut_a(sync_thread_levels_g(array, SYNC_TRX_UNDO));
		break;
	case SYNC_PURGE_LATCH:
		ut_a(sync_thread_levels_g(array, SYNC_PURGE_LATCH));
		break;
	case SYNC_PURGE_SYS:
		ut_a(sync_thread_levels_g(array, SYNC_PURGE_SYS));
		break;
	case SYNC_TREE_NODE:
		ut_a(sync_thread_levels_contain(array, SYNC_INDEX_TREE)
		     || sync_thread_levels_g(array, SYNC_TREE_NODE - 1));
		break;
	case SYNC_TREE_NODE_NEW:
		ut_a(sync_thread_levels_contain(array, SYNC_FSP_PAGE)
		     || sync_thread_levels_contain(array, SYNC_IBUF_MUTEX));
		break;
	case SYNC_INDEX_TREE:
		ut_a((sync_thread_levels_contain(array, SYNC_IBUF_MUTEX)
		      && sync_thread_levels_contain(array, SYNC_FSP)
		      && sync_thread_levels_g(array, SYNC_FSP_PAGE - 1))
		     || sync_thread_levels_g(array, SYNC_TREE_NODE - 1));
		break;
	case SYNC_IBUF_MUTEX:
		ut_a(sync_thread_levels_g(array, SYNC_FSP_PAGE - 1));
		break;
	case SYNC_IBUF_PESS_INSERT_MUTEX:
		ut_a(sync_thread_levels_g(array, SYNC_FSP - 1)
		     && !sync_thread_levels_contain(array, SYNC_IBUF_MUTEX));
		break;
	case SYNC_IBUF_HEADER:
		ut_a(sync_thread_levels_g(array, SYNC_FSP - 1)
		     && !sync_thread_levels_contain(array, SYNC_IBUF_MUTEX)
		     && !sync_thread_levels_contain
		     (array, SYNC_IBUF_PESS_INSERT_MUTEX));
		break;
	case SYNC_DICT_AUTOINC_MUTEX:
		ut_a(sync_thread_levels_g(array, SYNC_DICT_AUTOINC_MUTEX));
		break;
	case SYNC_DICT_OPERATION:
		ut_a(sync_thread_levels_g(array, SYNC_DICT_OPERATION));
		break;
	case SYNC_DICT_HEADER:
		ut_a(sync_thread_levels_g(array, SYNC_DICT_HEADER));
		break;
	case SYNC_DICT:
#ifdef UNIV_DEBUG
		ut_a(buf_debug_prints
		     || sync_thread_levels_g(array, SYNC_DICT));
#else /* UNIV_DEBUG */
		ut_a(sync_thread_levels_g(array, SYNC_DICT));
#endif /* UNIV_DEBUG */
		break;
	default:
		ut_error;
	}

	for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

		slot = sync_thread_levels_get_nth(array, i);

		if (slot->latch == NULL) {
			slot->latch = latch;
			slot->level = level;

			break;
		}
	}

	ut_a(i < SYNC_THREAD_N_LEVELS);

	mutex_exit(&sync_thread_mutex);
}

/**********************************************************************
Removes a latch from the thread level array if it is found there. */

ibool
sync_thread_reset_level(
/*====================*/
			/* out: TRUE if found from the array; it is an error
			if the latch is not found */
	void*	latch)	/* in: pointer to a mutex or an rw-lock */
{
	sync_level_t*	array;
	sync_level_t*	slot;
	sync_thread_t*	thread_slot;
	ulint		i;

	if (!sync_order_checks_on) {

		return(FALSE);
	}

	if ((latch == (void*)&sync_thread_mutex)
	    || (latch == (void*)&mutex_list_mutex)
#ifdef UNIV_SYNC_DEBUG
	    || (latch == (void*)&rw_lock_debug_mutex)
#endif /* UNIV_SYNC_DEBUG */
	    || (latch == (void*)&rw_lock_list_mutex)) {

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

	for (i = 0; i < SYNC_THREAD_N_LEVELS; i++) {

		slot = sync_thread_levels_get_nth(array, i);

		if (slot->latch == latch) {
			slot->latch = NULL;

			mutex_exit(&sync_thread_mutex);

			return(TRUE);
		}
	}

	ut_error;

	mutex_exit(&sync_thread_mutex);

	return(FALSE);
}

/**********************************************************************
Initializes the synchronization data structures. */

void
sync_init(void)
/*===========*/
{
	sync_thread_t*	thread_slot;
	ulint		i;

	ut_a(sync_initialized == FALSE);

	sync_initialized = TRUE;

	/* Create the primary system wait array which is protected by an OS
	mutex */

	sync_primary_wait_array = sync_array_create(OS_THREAD_MAX_N,
						    SYNC_ARRAY_OS_MUTEX);

	/* Create the thread latch level array where the latch levels
	are stored for each OS thread */

	sync_thread_level_arrays = ut_malloc(OS_THREAD_MAX_N
					     * sizeof(sync_thread_t));
	for (i = 0; i < OS_THREAD_MAX_N; i++) {

		thread_slot = sync_thread_level_arrays_get_nth(i);
		thread_slot->levels = NULL;
	}

	/* Init the mutex list and create the mutex to protect it. */

	UT_LIST_INIT(mutex_list);
	mutex_create(&mutex_list_mutex, SYNC_NO_ORDER_CHECK);

	mutex_create(&sync_thread_mutex, SYNC_NO_ORDER_CHECK);

	/* Init the rw-lock list and create the mutex to protect it. */

	UT_LIST_INIT(rw_lock_list);
	mutex_create(&rw_lock_list_mutex, SYNC_NO_ORDER_CHECK);

#ifdef UNIV_SYNC_DEBUG
	mutex_create(&rw_lock_debug_mutex, SYNC_NO_ORDER_CHECK);

	rw_lock_debug_event = os_event_create(NULL);
	rw_lock_debug_waiters = FALSE;
#endif /* UNIV_SYNC_DEBUG */
}

/**********************************************************************
Frees the resources in InnoDB's own synchronization data structures. Use
os_sync_free() after calling this. */

void
sync_close(void)
/*===========*/
{
	mutex_t*	mutex;

	sync_array_free(sync_primary_wait_array);

	mutex = UT_LIST_GET_FIRST(mutex_list);

	while (mutex) {
		mutex_free(mutex);
		mutex = UT_LIST_GET_FIRST(mutex_list);
	}

	mutex_free(&mutex_list_mutex);
	mutex_free(&sync_thread_mutex);
}

/***********************************************************************
Prints wait info of the sync system. */

void
sync_print_wait_info(
/*=================*/
	FILE*	file)		/* in: file where to print */
{
#ifdef UNIV_SYNC_DEBUG
	fprintf(stderr, "Mutex exits %lu, rws exits %lu, rwx exits %lu\n",
		mutex_exit_count, rw_s_exit_count, rw_x_exit_count);
#endif

	fprintf(file,
		"Mutex spin waits %lu, rounds %lu, OS waits %lu\n"
		"RW-shared spins %lu, OS waits %lu;"
		" RW-excl spins %lu, OS waits %lu\n",
		(ulong) mutex_spin_wait_count,
		(ulong) mutex_spin_round_count,
		(ulong) mutex_os_wait_count,
		(ulong) rw_s_spin_wait_count,
		(ulong) rw_s_os_wait_count,
		(ulong) rw_x_spin_wait_count,
		(ulong) rw_x_os_wait_count);
}

/***********************************************************************
Prints info of the sync system. */

void
sync_print(
/*=======*/
	FILE*	file)		/* in: file where to print */
{
#ifdef UNIV_SYNC_DEBUG
	mutex_list_print_info();

	rw_lock_list_print_info();
#endif /* UNIV_SYNC_DEBUG */

	sync_array_print_info(file, sync_primary_wait_array);

	sync_print_wait_info(file);
}
