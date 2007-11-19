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
ulint	rw_s_system_call_count	= 0;

/* number of spin waits on rw-latches,
resulted during shared (read) locks */
ulint	rw_s_spin_wait_count	= 0;

/* number of OS waits on rw-latches,
resulted during shared (read) locks */
ulint	rw_s_os_wait_count	= 0;

/* number of unlocks (that unlock shared locks),
set only when UNIV_SYNC_PERF_STAT is defined */
ulint	rw_s_exit_count		= 0;

/* number of system calls made during exclusive latching */
ulint	rw_x_system_call_count	= 0;

/* number of spin waits on rw-latches,
resulted during exclusive (write) locks */
ulint	rw_x_spin_wait_count	= 0;

/* number of OS waits on rw-latches,
resulted during exclusive (write) locks */
ulint	rw_x_os_wait_count	= 0;

/* number of unlocks (that unlock exclusive locks),
set only when UNIV_SYNC_PERF_STAT is defined */
ulint	rw_x_exit_count		= 0;

/* The global list of rw-locks */
rw_lock_list_t	rw_lock_list;
mutex_t		rw_lock_list_mutex;

#ifdef UNIV_SYNC_DEBUG
/* The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be
acquired in addition to the mutex protecting the lock. */

mutex_t		rw_lock_debug_mutex;
os_event_t	rw_lock_debug_event;	/* If deadlock detection does not
					get immediately the mutex, it may
					wait for this event */
ibool		rw_lock_debug_waiters;	/* This is set to TRUE, if there may
					be waiters for the event */

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

	mutex_create(rw_lock_get_mutex(lock), SYNC_NO_ORDER_CHECK);

	lock->mutex.cfile_name = cfile_name;
	lock->mutex.cline = cline;

#if defined UNIV_DEBUG && !defined UNIV_HOTBACKUP
	lock->mutex.cmutex_name = cmutex_name;
	lock->mutex.mutex_type = 1;
#endif /* UNIV_DEBUG && !UNIV_HOTBACKUP */

	rw_lock_set_waiters(lock, 0);
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

void
rw_lock_free(
/*=========*/
	rw_lock_t*	lock)	/* in: rw-lock */
{
	ut_ad(rw_lock_validate(lock));
	ut_a(rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED);
	ut_a(rw_lock_get_waiters(lock) == 0);
	ut_a(rw_lock_get_reader_count(lock) == 0);

	lock->magic_n = 0;

	mutex_free(rw_lock_get_mutex(lock));

	mutex_enter(&rw_lock_list_mutex);

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

ibool
rw_lock_validate(
/*=============*/
	rw_lock_t*	lock)
{
	ut_a(lock);

	mutex_enter(rw_lock_get_mutex(lock));

	ut_a(lock->magic_n == RW_LOCK_MAGIC_N);
	ut_a((rw_lock_get_reader_count(lock) == 0)
	     || (rw_lock_get_writer(lock) != RW_LOCK_EX));
	ut_a((rw_lock_get_writer(lock) == RW_LOCK_EX)
	     || (rw_lock_get_writer(lock) == RW_LOCK_WAIT_EX)
	     || (rw_lock_get_writer(lock) == RW_LOCK_NOT_LOCKED));
	ut_a((rw_lock_get_waiters(lock) == 0)
	     || (rw_lock_get_waiters(lock) == 1));
	ut_a((lock->writer != RW_LOCK_EX) || (lock->writer_count > 0));

	mutex_exit(rw_lock_get_mutex(lock));

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/**********************************************************************
Lock an rw-lock in shared mode for the current thread. If the rw-lock is
locked in exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. */

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
	rw_s_spin_wait_count++;

	/* Spin waiting for the writer field to become free */
	i = 0;

	while (rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED
	       && i < SYNC_SPIN_ROUNDS) {
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

	mutex_enter(rw_lock_get_mutex(lock));

	/* We try once again to obtain the lock */

	if (TRUE == rw_lock_s_lock_low(lock, pass, file_name, line)) {
		mutex_exit(rw_lock_get_mutex(lock));

		return; /* Success */
	} else {
		/* If we get here, locking did not succeed, we may
		suspend the thread to wait in the wait array */

		rw_s_system_call_count++;

		sync_array_reserve_cell(sync_primary_wait_array,
					lock, RW_LOCK_SHARED,
					file_name, line,
					&index);

		rw_lock_set_waiters(lock, 1);

		mutex_exit(rw_lock_get_mutex(lock));

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

void
rw_lock_x_lock_move_ownership(
/*==========================*/
	rw_lock_t*	lock)	/* in: lock which was x-locked in the
				buffer read */
{
	ut_ad(rw_lock_is_locked(lock, RW_LOCK_EX));

	mutex_enter(&(lock->mutex));

	lock->writer_thread = os_thread_get_curr_id();

	lock->pass = 0;

	mutex_exit(&(lock->mutex));
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
	ulint	state;	/* lock state acquired */
	ulint	i;	/* spin round count */

	ut_ad(rw_lock_validate(lock));

lock_loop:
	/* Acquire the mutex protecting the rw-lock fields */
	mutex_enter_fast(&(lock->mutex));

	state = rw_lock_x_lock_low(lock, pass, file_name, line);

	mutex_exit(&(lock->mutex));

	if (state == RW_LOCK_EX) {

		return;	/* Locking succeeded */

	} else if (state == RW_LOCK_NOT_LOCKED) {

		/* Spin waiting for the writer field to become free */
		i = 0;

		while (rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED
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
	} else if (state == RW_LOCK_WAIT_EX) {

		/* Spin waiting for the reader count field to become zero */
		i = 0;

		while (rw_lock_get_reader_count(lock) != 0
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
		i = 0; /* Eliminate a compiler warning */
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

	mutex_enter(rw_lock_get_mutex(lock));

	state = rw_lock_x_lock_low(lock, pass, file_name, line);

	if (state == RW_LOCK_EX) {
		mutex_exit(rw_lock_get_mutex(lock));

		return;	/* Locking succeeded */
	}

	rw_x_system_call_count++;

	sync_array_reserve_cell(sync_primary_wait_array,
				lock, RW_LOCK_EX,
				file_name, line,
				&index);

	rw_lock_set_waiters(lock, 1);

	mutex_exit(rw_lock_get_mutex(lock));

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

	mutex_enter(&(lock->mutex));

	info = UT_LIST_GET_FIRST(lock->debug_list);

	while (info != NULL) {

		if (os_thread_eq(info->thread_id, os_thread_get_curr_id())
		    && (info->pass == 0)
		    && (info->lock_type == lock_type)) {

			mutex_exit(&(lock->mutex));
			/* Found! */

			return(TRUE);
		}

		info = UT_LIST_GET_NEXT(list, info);
	}
	mutex_exit(&(lock->mutex));

	return(FALSE);
}
#endif /* UNIV_SYNC_DEBUG */

/**********************************************************************
Checks if somebody has locked the rw-lock in the specified mode. */

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

	mutex_enter(&(lock->mutex));

	if (lock_type == RW_LOCK_SHARED) {
		if (lock->reader_count > 0) {
			ret = TRUE;
		}
	} else if (lock_type == RW_LOCK_EX) {
		if (lock->writer == RW_LOCK_EX) {
			ret = TRUE;
		}
	} else {
		ut_error;
	}

	mutex_exit(&(lock->mutex));

	return(ret);
}

#ifdef UNIV_SYNC_DEBUG
/*******************************************************************
Prints debug info of currently locked rw-locks. */

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

		mutex_enter(&(lock->mutex));

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
		    || (rw_lock_get_reader_count(lock) != 0)
		    || (rw_lock_get_waiters(lock) != 0)) {

			fprintf(file, "RW-LOCK: %p ", (void*) lock);

			if (rw_lock_get_waiters(lock)) {
				fputs(" Waiters for the lock exist\n", file);
			} else {
				putc('\n', file);
			}

			info = UT_LIST_GET_FIRST(lock->debug_list);
			while (info != NULL) {
				rw_lock_debug_print(info);
				info = UT_LIST_GET_NEXT(list, info);
			}
		}

		mutex_exit(&(lock->mutex));
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	fprintf(file, "Total number of rw-locks %ld\n", count);
	mutex_exit(&rw_lock_list_mutex);
}

/*******************************************************************
Prints debug info of an rw-lock. */

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
	    || (rw_lock_get_waiters(lock) != 0)) {

		if (rw_lock_get_waiters(lock)) {
			fputs(" Waiters for the lock exist\n", stderr);
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

ulint
rw_lock_n_locked(void)
/*==================*/
{
	rw_lock_t*	lock;
	ulint		count		= 0;

	mutex_enter(&rw_lock_list_mutex);

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {
		mutex_enter(rw_lock_get_mutex(lock));

		if ((rw_lock_get_writer(lock) != RW_LOCK_NOT_LOCKED)
		    || (rw_lock_get_reader_count(lock) != 0)) {
			count++;
		}

		mutex_exit(rw_lock_get_mutex(lock));
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	mutex_exit(&rw_lock_list_mutex);

	return(count);
}
#endif /* UNIV_SYNC_DEBUG */
