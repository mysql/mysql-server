/******************************************************
The read-write lock (for thread synchronization)

(c) 1995 Innobase Oy

Created 9/11/1995 Heikki Tuuri
*******************************************************/

#include "sync0rw.h"
#ifdef UNIV_NONINL
#include "sync0rw.ic"
#endif

#include "os0thread.h"
#include "mem0mem.h"
#include "srv0srv.h"

/* number of system calls made during shared latching */
UNIV_INTERN ulint	rw_s_system_call_count	= 0;

/* number of spin waits on rw-latches,
resulted during shared (read) locks */
UNIV_INTERN ulint	rw_s_spin_wait_count	= 0;

/* number of OS waits on rw-latches,
resulted during shared (read) locks */
UNIV_INTERN ulint	rw_s_os_wait_count	= 0;

/* number of unlocks (that unlock shared locks),
set only when UNIV_SYNC_PERF_STAT is defined */
UNIV_INTERN ulint	rw_s_exit_count		= 0;

/* number of system calls made during exclusive latching */
UNIV_INTERN ulint	rw_x_system_call_count	= 0;

/* number of spin waits on rw-latches,
resulted during exclusive (write) locks */
UNIV_INTERN ulint	rw_x_spin_wait_count	= 0;

/* number of OS waits on rw-latches,
resulted during exclusive (write) locks */
UNIV_INTERN ulint	rw_x_os_wait_count	= 0;

/* number of unlocks (that unlock exclusive locks),
set only when UNIV_SYNC_PERF_STAT is defined */
UNIV_INTERN ulint	rw_x_exit_count		= 0;

/* The global list of rw-locks */
UNIV_INTERN rw_lock_list_t	rw_lock_list;
UNIV_INTERN mutex_t		rw_lock_list_mutex;

#ifdef UNIV_SYNC_DEBUG
/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be
acquired in addition to the mutex protecting the lock. */

UNIV_INTERN mutex_t		rw_lock_debug_mutex;
/* If deadlock detection does not get immediately the mutex,
it may wait for this event */
UNIV_INTERN os_event_t		rw_lock_debug_event;
/* This is set to TRUE, if there may be waiters for the event */
UNIV_INTERN ibool		rw_lock_debug_waiters;

/**********************************************************************
Creates a debug info struct. */
static
rw_lock_debug_t*
rw_lock_debug_create(void);
/*======================*/
/**********************************************************************
Frees a debug info struct. */
static
void
rw_lock_debug_free(
/*===============*/
	rw_lock_debug_t* info);

/**********************************************************************
Creates a debug info struct. */
static
rw_lock_debug_t*
rw_lock_debug_create(void)
/*======================*/
{
	return((rw_lock_debug_t*) mem_alloc(sizeof(rw_lock_debug_t)));
}

/**********************************************************************
Frees a debug info struct. */
static
void
rw_lock_debug_free(
/*===============*/
	rw_lock_debug_t* info)
{
	mem_free(info);
}
#endif /* UNIV_SYNC_DEBUG */

/**********************************************************************
Creates, or rather, initializes an rw-lock object in a specified memory
location (which must be appropriately aligned). The rw-lock is initialized
to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
is necessary only if the memory block containing it is freed. */
UNIV_INTERN
void
rw_lock_create_func(
/*================*/
	rw_lock_t*	lock,		/* in: pointer to memory */
#ifdef UNIV_DEBUG
# ifdef UNIV_SYNC_DEBUG
	ulint		level,		/* in: level */
# endif /* UNIV_SYNC_DEBUG */
	const char*	cmutex_name, 	/* in: mutex name */
#endif /* UNIV_DEBUG */
	const char*	cfile_name,	/* in: file name where created */
	ulint 		cline)		/* in: file line where created */
{
	/* If this is the very first time a synchronization object is
	created, then the following call initializes the sync system. */

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_create(rw_lock_get_mutex(lock), SYNC_NO_ORDER_CHECK);

	lock->mutex.cfile_name = cfile_name;
	lock->mutex.cline = cline;

#if defined UNIV_DEBUG && !defined UNIV_HOTBACKUP
	lock->mutex.cmutex_name = cmutex_name;
	lock->mutex.mutex_type = 1;
#endif /* UNIV_DEBUG && !UNIV_HOTBACKUP */
#endif /* !HAVE_GCC_ATOMIC_BUILTINS */

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	lock->lock_word = RW_LOCK_BIAS;
#endif
	rw_lock_set_s_waiters(lock, 0);
	rw_lock_set_x_waiters(lock, 0);
	rw_lock_set_wx_waiters(lock, 0);
	rw_lock_set_writer(lock, RW_LOCK_NOT_LOCKED);
	lock->writer_count = 0;
	rw_lock_set_reader_count(lock, 0);

	lock->writer_is_wait_ex = FALSE;

#ifdef UNIV_SYNC_DEBUG
	UT_LIST_INIT(lock->debug_list);

	lock->level = level;
#endif /* UNIV_SYNC_DEBUG */

	lock->magic_n = RW_LOCK_MAGIC_N;

	lock->cfile_name = cfile_name;
	lock->cline = (unsigned int) cline;

	lock->last_s_file_name = "not yet reserved";
	lock->last_x_file_name = "not yet reserved";
	lock->last_s_line = 0;
	lock->last_x_line = 0;
	lock->s_event = os_event_create(NULL);
	lock->x_event = os_event_create(NULL);
	lock->wait_ex_event = os_event_create(NULL);

	mutex_enter(&rw_lock_list_mutex);

	if (UT_LIST_GET_LEN(rw_lock_list) > 0) {
		ut_a(UT_LIST_GET_FIRST(rw_lock_list)->magic_n
		     == RW_LOCK_MAGIC_N);
	}

	UT_LIST_ADD_FIRST(list, rw_lock_list, lock);

	mutex_exit(&rw_lock_list_mutex);
}

/**********************************************************************
Calling this function is obligatory only if the memory buffer containing
the rw-lock is freed. Removes an rw-lock object from the global list. The
rw-lock is checked to be in the non-locked state. */
UNIV_INTERN
void
rw_lock_free(
/*=========*/
	rw_lock_t*	lock)	/* in: rw-lock */
{
	ut_ad(rw_lock_validate(lock));
	ut_a(rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED);
	ut_a(rw_lock_get_s_waiters(lock) == 0);
	ut_a(rw_lock_get_x_waiters(lock) == 0);
	ut_a(rw_lock_get_wx_waiters(lock) == 0);
	ut_a(rw_lock_get_reader_count(lock) == 0);

	lock->magic_n = 0;

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_free(rw_lock_get_mutex(lock));
#endif

	mutex_enter(&rw_lock_list_mutex);
	os_event_free(lock->s_event);
	os_event_free(lock->x_event);
	os_event_free(lock->wait_ex_event);

	if (UT_LIST_GET_PREV(list, lock)) {
		ut_a(UT_LIST_GET_PREV(list, lock)->magic_n == RW_LOCK_MAGIC_N);
	}
	if (UT_LIST_GET_NEXT(list, lock)) {
		ut_a(UT_LIST_GET_NEXT(list, lock)->magic_n == RW_LOCK_MAGIC_N);
	}

	UT_LIST_REMOVE(list, rw_lock_list, lock);

	mutex_exit(&rw_lock_list_mutex);
}

#ifdef UNIV_DEBUG
/**********************************************************************
Checks that the rw-lock has been initialized and that there are no
simultaneous shared and exclusive locks. */
/* MEMO: If HAVE_GCC_ATOMIC_BUILTINS, we should use this function statically. */
 
UNIV_INTERN
ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock)
{
	ulint	tmp;
	ut_a(lock);

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_enter(rw_lock_get_mutex(lock));
#endif

	ut_a(lock->magic_n == RW_LOCK_MAGIC_N);
#ifndef HAVE_GCC_ATOMIC_BUILTINS
	/* It is dynamic combination */
	ut_a((rw_lock_get_reader_count(lock) == 0)
	     || (rw_lock_get_writer(lock) != RW_LOCK_EX));
#endif
	tmp = rw_lock_get_writer(lock);
	ut_a((tmp == RW_LOCK_EX)
	     || (tmp == RW_LOCK_WAIT_EX)
	     || (tmp == RW_LOCK_NOT_LOCKED));
	tmp = rw_lock_get_s_waiters(lock);
	ut_a((tmp == 0) || (tmp == 1));
	tmp = rw_lock_get_x_waiters(lock);
	ut_a((tmp == 0) || (tmp == 1));
	tmp = rw_lock_get_wx_waiters(lock);
	ut_a((tmp == 0) || (tmp == 1));
#ifndef HAVE_GCC_ATOMIC_BUILTINS
	/* It is dynamic combination */
	ut_a((lock->writer != RW_LOCK_EX) || (lock->writer_count > 0));
#endif

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_exit(rw_lock_get_mutex(lock));
#endif

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/**********************************************************************
Lock an rw-lock in shared mode for the current thread. If the rw-lock is
locked in exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. */
UNIV_INTERN
void
rw_lock_s_lock_spin(
/*================*/
	rw_lock_t*	lock,	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock
				will be passed to another thread to unlock */
	const char*	file_name, /* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
	ulint	 index;	/* index of the reserved wait cell */
	ulint	 i;	/* spin round count */

	ut_ad(rw_lock_validate(lock));

lock_loop:
	i = 0;
spin_loop:
	rw_s_spin_wait_count++;

	/* Spin waiting for the writer field to become free */

	while (i < SYNC_SPIN_ROUNDS
	       && rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED) {
		if (srv_spin_wait_delay) {
			ut_delay(ut_rnd_interval(0, srv_spin_wait_delay));
		}

		i++;
	}

	if (i == SYNC_SPIN_ROUNDS) {
		os_thread_yield();
	}

	if (srv_print_latch_waits) {
		fprintf(stderr,
			"Thread %lu spin wait rw-s-lock at %p"
			" cfile %s cline %lu rnds %lu\n",
			(ulong) os_thread_pf(os_thread_get_curr_id()),
			(void*) lock,
			lock->cfile_name, (ulong) lock->cline, (ulong) i);
	}

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_enter(rw_lock_get_mutex(lock));
#endif

	/* We try once again to obtain the lock */

	if (TRUE == rw_lock_s_lock_low(lock, pass, file_name, line)) {
#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_exit(rw_lock_get_mutex(lock));
#endif

		return; /* Success */
	} else {
#ifdef HAVE_GCC_ATOMIC_BUILTINS
		/* like sync0sync.c doing */
		i++;

		if (i < SYNC_SPIN_ROUNDS) {
			goto spin_loop;
		}
#endif
		/* If we get here, locking did not succeed, we may
		suspend the thread to wait in the wait array */

		rw_s_system_call_count++;

		sync_array_reserve_cell(sync_primary_wait_array,
					lock, RW_LOCK_SHARED,
					file_name, line,
					&index);

		rw_lock_set_s_waiters(lock, 1);

#ifdef HAVE_GCC_ATOMIC_BUILTINS
		/* like sync0sync.c doing */
		for (i = 0; i < 4; i++) {
			if (TRUE == rw_lock_s_lock_low(lock, pass, file_name, line)) {
				sync_array_free_cell(sync_primary_wait_array, index);
				return; /* Success */
			}
		}
#else
		mutex_exit(rw_lock_get_mutex(lock));
#endif

		if (srv_print_latch_waits) {
			fprintf(stderr,
				"Thread %lu OS wait rw-s-lock at %p"
				" cfile %s cline %lu\n",
				os_thread_pf(os_thread_get_curr_id()),
				(void*) lock, lock->cfile_name,
				(ulong) lock->cline);
		}

		rw_s_system_call_count++;
		rw_s_os_wait_count++;

		sync_array_wait_event(sync_primary_wait_array, index);

		goto lock_loop;
	}
}

/**********************************************************************
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
	rw_lock_t*	lock)	/* in: lock which was x-locked in the
				buffer read */
{
	ut_ad(rw_lock_is_locked(lock, RW_LOCK_EX));

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_enter(&(lock->mutex));
#endif

	lock->writer_thread = os_thread_get_curr_id();

	lock->pass = 0;

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_exit(&(lock->mutex));
#else
	__sync_synchronize();
#endif
}

/**********************************************************************
Low-level function for acquiring an exclusive lock. */
UNIV_INLINE
ulint
rw_lock_x_lock_low(
/*===============*/
				/* out: RW_LOCK_NOT_LOCKED if did
				not succeed, RW_LOCK_EX if success,
				RW_LOCK_WAIT_EX, if got wait reservation */
	rw_lock_t*	lock,	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
#ifdef HAVE_GCC_ATOMIC_BUILTINS
	os_thread_id_t	curr_thread	= os_thread_get_curr_id();

	/* try to lock writer */
	if(__sync_lock_test_and_set(&(lock->writer),RW_LOCK_EX)
			== RW_LOCK_NOT_LOCKED) {
		/* success */
		/* obtain RW_LOCK_WAIT_EX right */
		lock->writer_thread = curr_thread;
		lock->pass = pass;
		lock->writer_is_wait_ex = TRUE;
		/* atomic operation may be safer about memory order. */
		__sync_synchronize();
#ifdef UNIV_SYNC_DEBUG
		rw_lock_add_debug_info(lock, pass, RW_LOCK_WAIT_EX,
					file_name, line);
#endif
	}

	if (!os_thread_eq(lock->writer_thread, curr_thread)) {
		return(RW_LOCK_NOT_LOCKED);
	}

	switch(rw_lock_get_writer(lock)) {
	    case RW_LOCK_WAIT_EX:
		/* have right to try x-lock */
		if (lock->lock_word == RW_LOCK_BIAS) {
			/* try x-lock */
			if(__sync_sub_and_fetch(&(lock->lock_word),
					RW_LOCK_BIAS) == 0) {
				/* success */
				lock->pass = pass;
				lock->writer_is_wait_ex = FALSE;
				__sync_fetch_and_add(&(lock->writer_count),1);

#ifdef UNIV_SYNC_DEBUG
				rw_lock_remove_debug_info(lock, pass, RW_LOCK_WAIT_EX);
				rw_lock_add_debug_info(lock, pass, RW_LOCK_EX,
							file_name, line);
#endif

				lock->last_x_file_name = file_name;
				lock->last_x_line = line;

				/* Locking succeeded, we may return */
				return(RW_LOCK_EX);
			} else {
				/* fail */
				__sync_fetch_and_add(&(lock->lock_word),
					RW_LOCK_BIAS);
			}
		}
		/* There are readers, we have to wait */
		return(RW_LOCK_WAIT_EX);

		break;

	    case RW_LOCK_EX:
		/* already have x-lock */
		if ((lock->pass == 0)&&(pass == 0)) {
			__sync_fetch_and_add(&(lock->writer_count),1);

#ifdef UNIV_SYNC_DEBUG
			rw_lock_add_debug_info(lock, pass, RW_LOCK_EX, file_name,
						line);
#endif

			lock->last_x_file_name = file_name;
			lock->last_x_line = line;

			/* Locking succeeded, we may return */
			return(RW_LOCK_EX);
		}

		return(RW_LOCK_NOT_LOCKED);

		break;

	    default: /* ??? */
		return(RW_LOCK_NOT_LOCKED);
	}
#else /* HAVE_GCC_ATOMIC_BUILTINS */

	ut_ad(mutex_own(rw_lock_get_mutex(lock)));

	if (rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED) {

		if (rw_lock_get_reader_count(lock) == 0) {

			rw_lock_set_writer(lock, RW_LOCK_EX);
			lock->writer_thread = os_thread_get_curr_id();
			lock->writer_count++;
			lock->pass = pass;

#ifdef UNIV_SYNC_DEBUG
			rw_lock_add_debug_info(lock, pass, RW_LOCK_EX,
					       file_name, line);
#endif
			lock->last_x_file_name = file_name;
			lock->last_x_line = (unsigned int) line;

			/* Locking succeeded, we may return */
			return(RW_LOCK_EX);
		} else {
			/* There are readers, we have to wait */
			rw_lock_set_writer(lock, RW_LOCK_WAIT_EX);
			lock->writer_thread = os_thread_get_curr_id();
			lock->pass = pass;
			lock->writer_is_wait_ex = TRUE;

#ifdef UNIV_SYNC_DEBUG
			rw_lock_add_debug_info(lock, pass, RW_LOCK_WAIT_EX,
					       file_name, line);
#endif

			return(RW_LOCK_WAIT_EX);
		}

	} else if ((rw_lock_get_writer(lock) == RW_LOCK_WAIT_EX)
		   && os_thread_eq(lock->writer_thread,
				   os_thread_get_curr_id())) {

		if (rw_lock_get_reader_count(lock) == 0) {

			rw_lock_set_writer(lock, RW_LOCK_EX);
			lock->writer_count++;
			lock->pass = pass;
			lock->writer_is_wait_ex = FALSE;

#ifdef UNIV_SYNC_DEBUG
			rw_lock_remove_debug_info(lock, pass, RW_LOCK_WAIT_EX);
			rw_lock_add_debug_info(lock, pass, RW_LOCK_EX,
					       file_name, line);
#endif

			lock->last_x_file_name = file_name;
			lock->last_x_line = (unsigned int) line;

			/* Locking succeeded, we may return */
			return(RW_LOCK_EX);
		}

		return(RW_LOCK_WAIT_EX);

	} else if ((rw_lock_get_writer(lock) == RW_LOCK_EX)
		   && os_thread_eq(lock->writer_thread,
				   os_thread_get_curr_id())
		   && (lock->pass == 0)
		   && (pass == 0)) {

		lock->writer_count++;

#ifdef UNIV_SYNC_DEBUG
		rw_lock_add_debug_info(lock, pass, RW_LOCK_EX, file_name,
				       line);
#endif

		lock->last_x_file_name = file_name;
		lock->last_x_line = (unsigned int) line;

		/* Locking succeeded, we may return */
		return(RW_LOCK_EX);
	}
#endif /* HAVE_GCC_ATOMIC_BUILTINS */

	/* Locking did not succeed */
	return(RW_LOCK_NOT_LOCKED);
}

/**********************************************************************
NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single x-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed! */
UNIV_INTERN
void
rw_lock_x_lock_func(
/*================*/
	rw_lock_t*	lock,	/* in: pointer to rw-lock */
	ulint		pass,	/* in: pass value; != 0, if the lock will
				be passed to another thread to unlock */
	const char*	file_name,/* in: file name where lock requested */
	ulint		line)	/* in: line where requested */
{
	ulint	index;	/* index of the reserved wait cell */
	ulint	state = RW_LOCK_NOT_LOCKED;	/* lock state acquired */
#ifdef HAVE_GCC_ATOMIC_BUILTINS
	ulint	prev_state = RW_LOCK_NOT_LOCKED;
#endif
	ulint	i;	/* spin round count */

	ut_ad(rw_lock_validate(lock));

lock_loop:
	i = 0;

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	prev_state = state;
#else
	/* Acquire the mutex protecting the rw-lock fields */
	mutex_enter_fast(&(lock->mutex));
#endif

	state = rw_lock_x_lock_low(lock, pass, file_name, line);

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	if (state != prev_state) i=0; /* if progress, reset counter. */
#else
	mutex_exit(&(lock->mutex));
#endif

spin_loop:
	if (state == RW_LOCK_EX) {

		return;	/* Locking succeeded */

	} else if (state == RW_LOCK_NOT_LOCKED) {

		/* Spin waiting for the writer field to become free */

		while (i < SYNC_SPIN_ROUNDS
		       && rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED) {
			if (srv_spin_wait_delay) {
				ut_delay(ut_rnd_interval(0,
							 srv_spin_wait_delay));
			}

			i++;
		}
		if (i == SYNC_SPIN_ROUNDS) {
			os_thread_yield();
		}
	} else if (state == RW_LOCK_WAIT_EX) {

		/* Spin waiting for the reader count field to become zero */

#ifdef HAVE_GCC_ATOMIC_BUILTINS
		while (lock->lock_word != RW_LOCK_BIAS
#else
		while (rw_lock_get_reader_count(lock) != 0
#endif
		       && i < SYNC_SPIN_ROUNDS) {
			if (srv_spin_wait_delay) {
				ut_delay(ut_rnd_interval(0,
							 srv_spin_wait_delay));
			}

			i++;
		}
		if (i == SYNC_SPIN_ROUNDS) {
			os_thread_yield();
		}
	} else {
		ut_error;
	}

	if (srv_print_latch_waits) {
		fprintf(stderr,
			"Thread %lu spin wait rw-x-lock at %p"
			" cfile %s cline %lu rnds %lu\n",
			os_thread_pf(os_thread_get_curr_id()), (void*) lock,
			lock->cfile_name, (ulong) lock->cline, (ulong) i);
	}

	rw_x_spin_wait_count++;

	/* We try once again to obtain the lock. Acquire the mutex protecting
	the rw-lock fields */

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	prev_state = state;
#else
	mutex_enter(rw_lock_get_mutex(lock));
#endif

	state = rw_lock_x_lock_low(lock, pass, file_name, line);

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	if (state != prev_state) i=0; /* if progress, reset counter. */
#endif

	if (state == RW_LOCK_EX) {
#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_exit(rw_lock_get_mutex(lock));
#endif

		return;	/* Locking succeeded */
	}

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	/* like sync0sync.c doing */
	i++;

	if (i < SYNC_SPIN_ROUNDS) {
		goto spin_loop;
	}
#endif

	rw_x_system_call_count++;

	sync_array_reserve_cell(sync_primary_wait_array,
				lock,
				(state == RW_LOCK_WAIT_EX)
				 ? RW_LOCK_WAIT_EX :
				RW_LOCK_EX,
				file_name, line,
				&index);

	if (state == RW_LOCK_WAIT_EX) {
		rw_lock_set_wx_waiters(lock, 1);
	} else {
		rw_lock_set_x_waiters(lock, 1);
	}

#ifdef HAVE_GCC_ATOMIC_BUILTINS
	/* like sync0sync.c doing */
	for (i = 0; i < 4; i++) {
		prev_state = state;
		state = rw_lock_x_lock_low(lock, pass, file_name, line);
		if (state == RW_LOCK_EX) {
			sync_array_free_cell(sync_primary_wait_array, index);
			return; /* Locking succeeded */
		}
		if (state != prev_state) {
			/* retry! */
			sync_array_free_cell(sync_primary_wait_array, index);
			goto lock_loop;
		}
	}
#else
	mutex_exit(rw_lock_get_mutex(lock));
#endif

	if (srv_print_latch_waits) {
		fprintf(stderr,
			"Thread %lu OS wait for rw-x-lock at %p"
			" cfile %s cline %lu\n",
			os_thread_pf(os_thread_get_curr_id()), (void*) lock,
			lock->cfile_name, (ulong) lock->cline);
	}

	rw_x_system_call_count++;
	rw_x_os_wait_count++;

	sync_array_wait_event(sync_primary_wait_array, index);

	goto lock_loop;
}

#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
UNIV_INTERN
void
rw_lock_debug_mutex_enter(void)
/*==========================*/
{
loop:
	if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
		return;
	}

	os_event_reset(rw_lock_debug_event);

	rw_lock_debug_waiters = TRUE;

	if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
		return;
	}

	os_event_wait(rw_lock_debug_event);

	goto loop;
}

/**********************************************************************
Releases the debug mutex. */
UNIV_INTERN
void
rw_lock_debug_mutex_exit(void)
/*==========================*/
{
	mutex_exit(&rw_lock_debug_mutex);

	if (rw_lock_debug_waiters) {
		rw_lock_debug_waiters = FALSE;
		os_event_set(rw_lock_debug_event);
	}
}

/**********************************************************************
Inserts the debug information for an rw-lock. */
UNIV_INTERN
void
rw_lock_add_debug_info(
/*===================*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		pass,		/* in: pass value */
	ulint		lock_type,	/* in: lock type */
	const char*	file_name,	/* in: file where requested */
	ulint		line)		/* in: line where requested */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);
	ut_ad(file_name);

	info = rw_lock_debug_create();

	rw_lock_debug_mutex_enter();

	info->file_name = file_name;
	info->line	= line;
	info->lock_type = lock_type;
	info->thread_id = os_thread_get_curr_id();
	info->pass	= pass;

	UT_LIST_ADD_FIRST(list, lock->debug_list, info);

	rw_lock_debug_mutex_exit();

	if ((pass == 0) && (lock_type != RW_LOCK_WAIT_EX)) {
		sync_thread_add_level(lock, lock->level);
	}
}

/**********************************************************************
Removes a debug information struct for an rw-lock. */
UNIV_INTERN
void
rw_lock_remove_debug_info(
/*======================*/
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		pass,		/* in: pass value */
	ulint		lock_type)	/* in: lock type */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);

	if ((pass == 0) && (lock_type != RW_LOCK_WAIT_EX)) {
		sync_thread_reset_level(lock);
	}

	rw_lock_debug_mutex_enter();

	info = UT_LIST_GET_FIRST(lock->debug_list);

	while (info != NULL) {
		if ((pass == info->pass)
		    && ((pass != 0)
			|| os_thread_eq(info->thread_id,
					os_thread_get_curr_id()))
		    && (info->lock_type == lock_type)) {

			/* Found! */
			UT_LIST_REMOVE(list, lock->debug_list, info);
			rw_lock_debug_mutex_exit();

			rw_lock_debug_free(info);

			return;
		}

		info = UT_LIST_GET_NEXT(list, info);
	}

	ut_error;
}
#endif /* UNIV_SYNC_DEBUG */

#ifdef UNIV_SYNC_DEBUG
/**********************************************************************
Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. */
UNIV_INTERN
ibool
rw_lock_own(
/*========*/
					/* out: TRUE if locked */
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type)	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
{
	rw_lock_debug_t*	info;

	ut_ad(lock);
	ut_ad(rw_lock_validate(lock));

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_enter(&(lock->mutex));
#endif

	info = UT_LIST_GET_FIRST(lock->debug_list);

	while (info != NULL) {

		if (os_thread_eq(info->thread_id, os_thread_get_curr_id())
		    && (info->pass == 0)
		    && (info->lock_type == lock_type)) {

#ifndef HAVE_GCC_ATOMIC_BUILTINS
			mutex_exit(&(lock->mutex));
#endif
			/* Found! */

			return(TRUE);
		}

		info = UT_LIST_GET_NEXT(list, info);
	}
#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_exit(&(lock->mutex));
#endif

	return(FALSE);
}
#endif /* UNIV_SYNC_DEBUG */

/**********************************************************************
Checks if somebody has locked the rw-lock in the specified mode. */
UNIV_INTERN
ibool
rw_lock_is_locked(
/*==============*/
					/* out: TRUE if locked */
	rw_lock_t*	lock,		/* in: rw-lock */
	ulint		lock_type)	/* in: lock type: RW_LOCK_SHARED,
					RW_LOCK_EX */
{
	ibool	ret	= FALSE;

	ut_ad(lock);
	ut_ad(rw_lock_validate(lock));

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_enter(&(lock->mutex));
#endif

	if (lock_type == RW_LOCK_SHARED) {
		if (lock->reader_count > 0) {
			ret = TRUE;
		}
	} else if (lock_type == RW_LOCK_EX) {
		if (rw_lock_get_writer(lock) == RW_LOCK_EX) {
			ret = TRUE;
		}
	} else {
		ut_error;
	}

#ifndef HAVE_GCC_ATOMIC_BUILTINS
	mutex_exit(&(lock->mutex));
#endif

	return(ret);
}

#ifdef UNIV_SYNC_DEBUG
/*******************************************************************
Prints debug info of currently locked rw-locks. */
UNIV_INTERN
void
rw_lock_list_print_info(
/*====================*/
	FILE*	file)		/* in: file where to print */
{
	rw_lock_t*	lock;
	ulint		count		= 0;
	rw_lock_debug_t* info;

	mutex_enter(&rw_lock_list_mutex);

	fputs("-------------\n"
	      "RW-LATCH INFO\n"
	      "-------------\n", file);

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {

		count++;

#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_enter(&(lock->mutex));
#endif

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
		    || (rw_lock_get_reader_count(lock) != 0)
		    || (rw_lock_get_s_waiters(lock) != 0)
		    || (rw_lock_get_x_waiters(lock) != 0)
		    || (rw_lock_get_wx_waiters(lock) != 0)) {

			fprintf(file, "RW-LOCK: %p ", (void*) lock);

			if (rw_lock_get_s_waiters(lock)) {
				fputs(" s_waiters for the lock exist,", file);
			}
			if (rw_lock_get_x_waiters(lock)) {
				fputs(" x_waiters for the lock exist\n", file);
			}
			if (rw_lock_get_wx_waiters(lock)) {
				fputs(" wait_ex_waiters for the lock exist\n", file);
			} else {
				putc('\n', file);
			}

			info = UT_LIST_GET_FIRST(lock->debug_list);
			while (info != NULL) {
				rw_lock_debug_print(info);
				info = UT_LIST_GET_NEXT(list, info);
			}
		}

#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_exit(&(lock->mutex));
#endif
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	fprintf(file, "Total number of rw-locks %ld\n", count);
	mutex_exit(&rw_lock_list_mutex);
}

/*******************************************************************
Prints debug info of an rw-lock. */
UNIV_INTERN
void
rw_lock_print(
/*==========*/
	rw_lock_t*	lock)	/* in: rw-lock */
{
	rw_lock_debug_t* info;

	fprintf(stderr,
		"-------------\n"
		"RW-LATCH INFO\n"
		"RW-LATCH: %p ", (void*) lock);

	if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
	    || (rw_lock_get_reader_count(lock) != 0)
	    || (rw_lock_get_s_waiters(lock) != 0)
	    || (rw_lock_get_x_waiters(lock) != 0)
	    || (rw_lock_get_wx_waiters(lock) != 0)) {

		if (rw_lock_get_s_waiters(lock)) {
			fputs(" s_waiters for the lock exist,", stderr);
		}
		if (rw_lock_get_x_waiters(lock)) {
			fputs(" x_waiters for the lock exist\n", stderr);
		}
		if (rw_lock_get_wx_waiters(lock)) {
			fputs(" wait_ex_waiters for the lock exist\n", stderr);
		} else {
			putc('\n', stderr);
		}

		info = UT_LIST_GET_FIRST(lock->debug_list);
		while (info != NULL) {
			rw_lock_debug_print(info);
			info = UT_LIST_GET_NEXT(list, info);
		}
	}
}

/*************************************************************************
Prints info of a debug struct. */
UNIV_INTERN
void
rw_lock_debug_print(
/*================*/
	rw_lock_debug_t*	info)	/* in: debug struct */
{
	ulint	rwt;

	rwt	  = info->lock_type;

	fprintf(stderr, "Locked: thread %ld file %s line %ld  ",
		(ulong) os_thread_pf(info->thread_id), info->file_name,
		(ulong) info->line);
	if (rwt == RW_LOCK_SHARED) {
		fputs("S-LOCK", stderr);
	} else if (rwt == RW_LOCK_EX) {
		fputs("X-LOCK", stderr);
	} else if (rwt == RW_LOCK_WAIT_EX) {
		fputs("WAIT X-LOCK", stderr);
	} else {
		ut_error;
	}
	if (info->pass != 0) {
		fprintf(stderr, " pass value %lu", (ulong) info->pass);
	}
	putc('\n', stderr);
}

/*******************************************************************
Returns the number of currently locked rw-locks. Works only in the debug
version. */
UNIV_INTERN
ulint
rw_lock_n_locked(void)
/*==================*/
{
	rw_lock_t*	lock;
	ulint		count		= 0;

	mutex_enter(&rw_lock_list_mutex);

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {
#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_enter(rw_lock_get_mutex(lock));
#endif

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
		    || (rw_lock_get_reader_count(lock) != 0)) {
			count++;
		}

#ifndef HAVE_GCC_ATOMIC_BUILTINS
		mutex_exit(rw_lock_get_mutex(lock));
#endif
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	mutex_exit(&rw_lock_list_mutex);

	return(count);
}
#endif /* UNIV_SYNC_DEBUG */
