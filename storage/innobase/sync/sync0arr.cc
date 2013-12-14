/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.
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
@file sync/sync0arr.cc
The wait array used in synchronization primitives

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#include "sync0arr.h"
#ifdef UNIV_NONINL
#include "sync0arr.ic"
#endif

#include "sync0sync.h"
#include "sync0rw.h"
#include "os0sync.h"
#include "os0file.h"
#include "lock0lock.h"
#include "srv0srv.h"
#include "ha_prototypes.h"

/*
			WAIT ARRAY
			==========

The wait array consists of cells each of which has an
an operating system event object created for it. The threads
waiting for a mutex, for example, can reserve a cell
in the array and suspend themselves to wait for the event
to become signaled. When using the wait array, remember to make
sure that some thread holding the synchronization object
will eventually know that there is a waiter in the array and
signal the object, to prevent infinite wait.
Why we chose to implement a wait array? First, to make
mutexes fast, we had to code our own implementation of them,
which only in usually uncommon cases resorts to using
slow operating system primitives. Then we had the choice of
assigning a unique OS event for each mutex, which would
be simpler, or using a global wait array. In some operating systems,
the global wait array solution is more efficient and flexible,
because we can do with a very small number of OS events,
say 200. In NT 3.51, allocating events seems to be a quadratic
algorithm, because 10 000 events are created fast, but
100 000 events takes a couple of minutes to create.

As of 5.0.30 the above mentioned design is changed. Since now
OS can handle millions of wait events efficiently, we no longer
have this concept of each cell of wait array having one event.
Instead, now the event that a thread wants to wait on is embedded
in the wait object (mutex or rw_lock). We still keep the global
wait array for the sake of diagnostics and also to avoid infinite
wait The error_monitor thread scans the global wait array to signal
any waiting threads who have missed the signal. */

/** A cell where an individual thread may wait suspended
until a resource is released. The suspending is implemented
using an operating system event semaphore. */
struct sync_cell_t {
	void*		wait_object;	/*!< pointer to the object the
					thread is waiting for; if NULL
					the cell is free for use */
	ib_mutex_t*	old_wait_mutex;	/*!< the latest wait mutex in cell */
	rw_lock_t*	old_wait_rw_lock;
					/*!< the latest wait rw-lock
					in cell */
	ulint		request_type;	/*!< lock type requested on the
					object */
	const char*	file;		/*!< in debug version file where
					requested */
	ulint		line;		/*!< in debug version line where
					requested */
	os_thread_id_t	thread;		/*!< thread id of this waiting
					thread */
	ibool		waiting;	/*!< TRUE if the thread has already
					called sync_array_event_wait
					on this cell */
	ib_int64_t	signal_count;	/*!< We capture the signal_count
					of the wait_object when we
					reset the event. This value is
					then passed on to os_event_wait
					and we wait only if the event
					has not been signalled in the
					period between the reset and
					wait call. */
	time_t		reservation_time;/*!< time when the thread reserved
					the wait cell */
};

/* NOTE: It is allowed for a thread to wait
for an event allocated for the array without owning the
protecting mutex (depending on the case: OS or database mutex), but
all changes (set or reset) to the state of the event must be made
while owning the mutex. */

/** Synchronization array */
struct sync_array_t {
	ulint		n_reserved;	/*!< number of currently reserved
					cells in the wait array */
	ulint		n_cells;	/*!< number of cells in the
					wait array */
	sync_cell_t*	array;		/*!< pointer to wait array */
	ib_mutex_t	mutex;		/*!< possible database mutex
					protecting this data structure */
	os_ib_mutex_t	os_mutex;	/*!< Possible operating system mutex
					protecting the data structure.
					As this data structure is used in
					constructing the database mutex,
					to prevent infinite recursion
					in implementation, we fall back to
					an OS mutex. */
	ulint		res_count;	/*!< count of cell reservations
					since creation of the array */
};

/** User configured sync array size */
UNIV_INTERN ulong	srv_sync_array_size = 32;

/** Locally stored copy of srv_sync_array_size */
static	ulint		sync_array_size;

/** The global array of wait cells for implementation of the database's own
mutexes and read-write locks */
static	sync_array_t**	sync_wait_array;

/** count of how many times an object has been signalled */
static ulint		sg_count;

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
This function is called only in the debug version. Detects a deadlock
of one or more threads because of waits of semaphores.
@return	TRUE if deadlock detected */
static
ibool
sync_array_detect_deadlock(
/*=======================*/
	sync_array_t*	arr,	/*!< in: wait array; NOTE! the caller must
				own the mutex to array */
	sync_cell_t*	start,	/*!< in: cell where recursive search started */
	sync_cell_t*	cell,	/*!< in: cell to search */
	ulint		depth);	/*!< in: recursion depth */
#endif /* UNIV_SYNC_DEBUG */

/*****************************************************************//**
Gets the nth cell in array.
@return	cell */
static
sync_cell_t*
sync_array_get_nth_cell(
/*====================*/
	sync_array_t*	arr,	/*!< in: sync array */
	ulint		n)	/*!< in: index */
{
	ut_a(arr);
	ut_a(n < arr->n_cells);

	return(arr->array + n);
}

/******************************************************************//**
Reserves the mutex semaphore protecting a sync array. */
static
void
sync_array_enter(
/*=============*/
	sync_array_t*	arr)	/*!< in: sync wait array */
{
	os_mutex_enter(arr->os_mutex);
}

/******************************************************************//**
Releases the mutex semaphore protecting a sync array. */
static
void
sync_array_exit(
/*============*/
	sync_array_t*	arr)	/*!< in: sync wait array */
{
	os_mutex_exit(arr->os_mutex);
}

/*******************************************************************//**
Creates a synchronization wait array. It is protected by a mutex
which is automatically reserved when the functions operating on it
are called.
@return	own: created wait array */
static
sync_array_t*
sync_array_create(
/*==============*/
	ulint	n_cells)	/*!< in: number of cells in the array
				to create */
{
	ulint		sz;
	sync_array_t*	arr;

	ut_a(n_cells > 0);

	/* Allocate memory for the data structures */
	arr = static_cast<sync_array_t*>(ut_malloc(sizeof(*arr)));
	memset(arr, 0x0, sizeof(*arr));

	sz = sizeof(sync_cell_t) * n_cells;
	arr->array = static_cast<sync_cell_t*>(ut_malloc(sz));
	memset(arr->array, 0x0, sz);

	arr->n_cells = n_cells;

	/* Then create the mutex to protect the wait array complex */
	arr->os_mutex = os_mutex_create();

	return(arr);
}

/******************************************************************//**
Frees the resources in a wait array. */
static
void
sync_array_free(
/*============*/
	sync_array_t*	arr)	/*!< in, own: sync wait array */
{
	ut_a(arr->n_reserved == 0);

	sync_array_validate(arr);

	/* Release the mutex protecting the wait array complex */

	os_mutex_free(arr->os_mutex);

	ut_free(arr->array);
	ut_free(arr);
}

/********************************************************************//**
Validates the integrity of the wait array. Checks
that the number of reserved cells equals the count variable. */
UNIV_INTERN
void
sync_array_validate(
/*================*/
	sync_array_t*	arr)	/*!< in: sync wait array */
{
	ulint		i;
	sync_cell_t*	cell;
	ulint		count		= 0;

	sync_array_enter(arr);

	for (i = 0; i < arr->n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);
		if (cell->wait_object != NULL) {
			count++;
		}
	}

	ut_a(count == arr->n_reserved);

	sync_array_exit(arr);
}

/*******************************************************************//**
Returns the event that the thread owning the cell waits for. */
static
os_event_t
sync_cell_get_event(
/*================*/
	sync_cell_t*	cell) /*!< in: non-empty sync array cell */
{
	ulint type = cell->request_type;

	if (type == SYNC_MUTEX) {
		return(((ib_mutex_t*) cell->wait_object)->event);
	} else if (type == RW_LOCK_WAIT_EX) {
		return(((rw_lock_t*) cell->wait_object)->wait_ex_event);
	} else { /* RW_LOCK_SHARED and RW_LOCK_EX wait on the same event */
		return(((rw_lock_t*) cell->wait_object)->event);
	}
}

/******************************************************************//**
Reserves a wait array cell for waiting for an object.
The event of the cell is reset to nonsignalled state.
@return true if free cell is found, otherwise false */
UNIV_INTERN
bool
sync_array_reserve_cell(
/*====================*/
	sync_array_t*	arr,	/*!< in: wait array */
	void*		object, /*!< in: pointer to the object to wait for */
	ulint		type,	/*!< in: lock request type */
	const char*	file,	/*!< in: file where requested */
	ulint		line,	/*!< in: line where requested */
	ulint*		index)	/*!< out: index of the reserved cell */
{
	sync_cell_t*	cell;
	os_event_t      event;
	ulint		i;

	ut_a(object);
	ut_a(index);

	sync_array_enter(arr);

	arr->res_count++;

	/* Reserve a new cell. */
	for (i = 0; i < arr->n_cells; i++) {
		cell = sync_array_get_nth_cell(arr, i);

		if (cell->wait_object == NULL) {

			cell->waiting = FALSE;
			cell->wait_object = object;

			if (type == SYNC_MUTEX) {
				cell->old_wait_mutex =
					static_cast<ib_mutex_t*>(object);
			} else {
				cell->old_wait_rw_lock =
					static_cast<rw_lock_t*>(object);
			}

			cell->request_type = type;

			cell->file = file;
			cell->line = line;

			arr->n_reserved++;

			*index = i;

			sync_array_exit(arr);

			/* Make sure the event is reset and also store
			the value of signal_count at which the event
			was reset. */
                        event = sync_cell_get_event(cell);
			cell->signal_count = os_event_reset(event);

			cell->reservation_time = ut_time();

			cell->thread = os_thread_get_curr_id();

			return(true);
		}
	}

	/* No free cell found */
	return false;
}

/******************************************************************//**
This function should be called when a thread starts to wait on
a wait array cell. In the debug version this function checks
if the wait for a semaphore will result in a deadlock, in which
case prints info and asserts. */
UNIV_INTERN
void
sync_array_wait_event(
/*==================*/
	sync_array_t*	arr,	/*!< in: wait array */
	ulint		index)	/*!< in: index of the reserved cell */
{
	sync_cell_t*	cell;
	os_event_t	event;

	ut_a(arr);

	sync_array_enter(arr);

	cell = sync_array_get_nth_cell(arr, index);

	ut_a(cell->wait_object);
	ut_a(!cell->waiting);
	ut_ad(os_thread_get_curr_id() == cell->thread);

	event = sync_cell_get_event(cell);
	cell->waiting = TRUE;

#ifdef UNIV_SYNC_DEBUG

	/* We use simple enter to the mutex below, because if
	we cannot acquire it at once, mutex_enter would call
	recursively sync_array routines, leading to trouble.
	rw_lock_debug_mutex freezes the debug lists. */

	rw_lock_debug_mutex_enter();

	if (TRUE == sync_array_detect_deadlock(arr, cell, cell, 0)) {

		fputs("########################################\n", stderr);
		ut_error;
	}

	rw_lock_debug_mutex_exit();
#endif
	sync_array_exit(arr);

	os_event_wait_low(event, cell->signal_count);

	sync_array_free_cell(arr, index);
}

/******************************************************************//**
Reports info of a wait array cell. */
static
void
sync_array_cell_print(
/*==================*/
	FILE*		file,	/*!< in: file where to print */
	sync_cell_t*	cell)	/*!< in: sync cell */
{
	ib_mutex_t*	mutex;
	rw_lock_t*	rwlock;
	ulint		type;
	ulint		writer;

	type = cell->request_type;

	fprintf(file,
		"--Thread %lu has waited at %s line %lu"
		" for %.2f seconds the semaphore:\n",
		(ulong) os_thread_pf(cell->thread),
		innobase_basename(cell->file), (ulong) cell->line,
		difftime(time(NULL), cell->reservation_time));

	if (type == SYNC_MUTEX) {
		/* We use old_wait_mutex in case the cell has already
		been freed meanwhile */
		mutex = cell->old_wait_mutex;

		fprintf(file,
			"Mutex at %p created file %s line %lu, lock var %lu\n"
#ifdef UNIV_SYNC_DEBUG
			"Last time reserved in file %s line %lu, "
#endif /* UNIV_SYNC_DEBUG */
			"waiters flag %lu\n",
			(void*) mutex, innobase_basename(mutex->cfile_name),
			(ulong) mutex->cline,
			(ulong) mutex->lock_word,
#ifdef UNIV_SYNC_DEBUG
			mutex->file_name, (ulong) mutex->line,
#endif /* UNIV_SYNC_DEBUG */
			(ulong) mutex->waiters);

	} else if (type == RW_LOCK_EX
		   || type == RW_LOCK_WAIT_EX
		   || type == RW_LOCK_SHARED) {

		fputs(type == RW_LOCK_EX ? "X-lock on"
		      : type == RW_LOCK_WAIT_EX ? "X-lock (wait_ex) on"
		      : "S-lock on", file);

		rwlock = cell->old_wait_rw_lock;

		fprintf(file,
			" RW-latch at %p created in file %s line %lu\n",
			(void*) rwlock, innobase_basename(rwlock->cfile_name),
			(ulong) rwlock->cline);
		writer = rw_lock_get_writer(rwlock);
		if (writer != RW_LOCK_NOT_LOCKED) {
			fprintf(file,
				"a writer (thread id %lu) has"
				" reserved it in mode %s",
				(ulong) os_thread_pf(rwlock->writer_thread),
				writer == RW_LOCK_EX
				? " exclusive\n"
				: " wait exclusive\n");
		}

		fprintf(file,
			"number of readers %lu, waiters flag %lu, "
                        "lock_word: %lx\n"
			"Last time read locked in file %s line %lu\n"
			"Last time write locked in file %s line %lu\n",
			(ulong) rw_lock_get_reader_count(rwlock),
			(ulong) rwlock->waiters,
			rwlock->lock_word,
			innobase_basename(rwlock->last_s_file_name),
			(ulong) rwlock->last_s_line,
			rwlock->last_x_file_name,
			(ulong) rwlock->last_x_line);
	} else {
		ut_error;
	}

	if (!cell->waiting) {
		fputs("wait has ended\n", file);
	}
}

#ifdef UNIV_SYNC_DEBUG
/******************************************************************//**
Looks for a cell with the given thread id.
@return	pointer to cell or NULL if not found */
static
sync_cell_t*
sync_array_find_thread(
/*===================*/
	sync_array_t*	arr,	/*!< in: wait array */
	os_thread_id_t	thread)	/*!< in: thread id */
{
	ulint		i;
	sync_cell_t*	cell;

	for (i = 0; i < arr->n_cells; i++) {

		cell = sync_array_get_nth_cell(arr, i);

		if (cell->wait_object != NULL
		    && os_thread_eq(cell->thread, thread)) {

			return(cell);	/* Found */
		}
	}

	return(NULL);	/* Not found */
}

/******************************************************************//**
Recursion step for deadlock detection.
@return	TRUE if deadlock detected */
static
ibool
sync_array_deadlock_step(
/*=====================*/
	sync_array_t*	arr,	/*!< in: wait array; NOTE! the caller must
				own the mutex to array */
	sync_cell_t*	start,	/*!< in: cell where recursive search
				started */
	os_thread_id_t	thread,	/*!< in: thread to look at */
	ulint		pass,	/*!< in: pass value */
	ulint		depth)	/*!< in: recursion depth */
{
	sync_cell_t*	new_cell;

	if (pass != 0) {
		/* If pass != 0, then we do not know which threads are
		responsible of releasing the lock, and no deadlock can
		be detected. */

		return(FALSE);
	}

	new_cell = sync_array_find_thread(arr, thread);

	if (new_cell == start) {
		/* Deadlock */
		fputs("########################################\n"
		      "DEADLOCK of threads detected!\n", stderr);

		return(TRUE);

	} else if (new_cell) {
		return(sync_array_detect_deadlock(
			arr, start, new_cell, depth + 1));
	}
	return(FALSE);
}

/******************************************************************//**
This function is called only in the debug version. Detects a deadlock
of one or more threads because of waits of semaphores.
@return	TRUE if deadlock detected */
static
ibool
sync_array_detect_deadlock(
/*=======================*/
	sync_array_t*	arr,	/*!< in: wait array; NOTE! the caller must
				own the mutex to array */
	sync_cell_t*	start,	/*!< in: cell where recursive search started */
	sync_cell_t*	cell,	/*!< in: cell to search */
	ulint		depth)	/*!< in: recursion depth */
{
	ib_mutex_t*	mutex;
	rw_lock_t*	lock;
	os_thread_id_t	thread;
	ibool		ret;
	rw_lock_debug_t*debug;

	ut_a(arr);
	ut_a(start);
	ut_a(cell);
	ut_ad(cell->wait_object);
	ut_ad(os_thread_get_curr_id() == start->thread);
	ut_ad(depth < 100);

	depth++;

	if (!cell->waiting) {

		return(FALSE); /* No deadlock here */
	}

	if (cell->request_type == SYNC_MUTEX) {

		mutex = static_cast<ib_mutex_t*>(cell->wait_object);

		if (mutex_get_lock_word(mutex) != 0) {

			thread = mutex->thread_id;

			/* Note that mutex->thread_id above may be
			also OS_THREAD_ID_UNDEFINED, because the
			thread which held the mutex maybe has not
			yet updated the value, or it has already
			released the mutex: in this case no deadlock
			can occur, as the wait array cannot contain
			a thread with ID_UNDEFINED value. */

			ret = sync_array_deadlock_step(arr, start, thread, 0,
						       depth);
			if (ret) {
				fprintf(stderr,
			"Mutex %p owned by thread %lu file %s line %lu\n",
					mutex, (ulong) os_thread_pf(mutex->thread_id),
					mutex->file_name, (ulong) mutex->line);
				sync_array_cell_print(stderr, cell);

				return(TRUE);
			}
		}

		return(FALSE); /* No deadlock */

	} else if (cell->request_type == RW_LOCK_EX
		   || cell->request_type == RW_LOCK_WAIT_EX) {

		lock = static_cast<rw_lock_t*>(cell->wait_object);

		for (debug = UT_LIST_GET_FIRST(lock->debug_list);
		     debug != 0;
		     debug = UT_LIST_GET_NEXT(list, debug)) {

			thread = debug->thread_id;

			if (((debug->lock_type == RW_LOCK_EX)
			     && !os_thread_eq(thread, cell->thread))
			    || ((debug->lock_type == RW_LOCK_WAIT_EX)
				&& !os_thread_eq(thread, cell->thread))
			    || (debug->lock_type == RW_LOCK_SHARED)) {

				/* The (wait) x-lock request can block
				infinitely only if someone (can be also cell
				thread) is holding s-lock, or someone
				(cannot be cell thread) (wait) x-lock, and
				he is blocked by start thread */

				ret = sync_array_deadlock_step(
					arr, start, thread, debug->pass,
					depth);
				if (ret) {
print:
					fprintf(stderr, "rw-lock %p ",
						(void*) lock);
					sync_array_cell_print(stderr, cell);
					rw_lock_debug_print(stderr, debug);
					return(TRUE);
				}
			}
		}

		return(FALSE);

	} else if (cell->request_type == RW_LOCK_SHARED) {

		lock = static_cast<rw_lock_t*>(cell->wait_object);

		for (debug = UT_LIST_GET_FIRST(lock->debug_list);
		     debug != 0;
		     debug = UT_LIST_GET_NEXT(list, debug)) {

			thread = debug->thread_id;

			if ((debug->lock_type == RW_LOCK_EX)
			    || (debug->lock_type == RW_LOCK_WAIT_EX)) {

				/* The s-lock request can block infinitely
				only if someone (can also be cell thread) is
				holding (wait) x-lock, and he is blocked by
				start thread */

				ret = sync_array_deadlock_step(
					arr, start, thread, debug->pass,
					depth);
				if (ret) {
					goto print;
				}
			}
		}

		return(FALSE);

	} else {
		ut_error;
	}

	return(TRUE);	/* Execution never reaches this line: for compiler
			fooling only */
}
#endif /* UNIV_SYNC_DEBUG */

/******************************************************************//**
Determines if we can wake up the thread waiting for a sempahore. */
static
ibool
sync_arr_cell_can_wake_up(
/*======================*/
	sync_cell_t*	cell)	/*!< in: cell to search */
{
	ib_mutex_t*	mutex;
	rw_lock_t*	lock;

	if (cell->request_type == SYNC_MUTEX) {

		mutex = static_cast<ib_mutex_t*>(cell->wait_object);

		if (mutex_get_lock_word(mutex) == 0) {

			return(TRUE);
		}

	} else if (cell->request_type == RW_LOCK_EX) {

		lock = static_cast<rw_lock_t*>(cell->wait_object);

		if (lock->lock_word > 0) {
		/* Either unlocked or only read locked. */

			return(TRUE);
		}

        } else if (cell->request_type == RW_LOCK_WAIT_EX) {

		lock = static_cast<rw_lock_t*>(cell->wait_object);

                /* lock_word == 0 means all readers have left */
		if (lock->lock_word == 0) {

			return(TRUE);
		}
	} else if (cell->request_type == RW_LOCK_SHARED) {
		lock = static_cast<rw_lock_t*>(cell->wait_object);

                /* lock_word > 0 means no writer or reserved writer */
		if (lock->lock_word > 0) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/******************************************************************//**
Frees the cell. NOTE! sync_array_wait_event frees the cell
automatically! */
UNIV_INTERN
void
sync_array_free_cell(
/*=================*/
	sync_array_t*	arr,	/*!< in: wait array */
	ulint		index)  /*!< in: index of the cell in array */
{
	sync_cell_t*	cell;

	sync_array_enter(arr);

	cell = sync_array_get_nth_cell(arr, index);

	ut_a(cell->wait_object != NULL);

	cell->waiting = FALSE;
	cell->wait_object =  NULL;
	cell->signal_count = 0;

	ut_a(arr->n_reserved > 0);
	arr->n_reserved--;

	sync_array_exit(arr);
}

/**********************************************************************//**
Increments the signalled count. */
UNIV_INTERN
void
sync_array_object_signalled(void)
/*=============================*/
{
#ifdef HAVE_ATOMIC_BUILTINS
	(void) os_atomic_increment_ulint(&sg_count, 1);
#else
	++sg_count;
#endif /* HAVE_ATOMIC_BUILTINS */
}

/**********************************************************************//**
If the wakeup algorithm does not work perfectly at semaphore relases,
this function will do the waking (see the comment in mutex_exit). This
function should be called about every 1 second in the server.

Note that there's a race condition between this thread and mutex_exit
changing the lock_word and calling signal_object, so sometimes this finds
threads to wake up even when nothing has gone wrong. */
static
void
sync_array_wake_threads_if_sema_free_low(
/*=====================================*/
	sync_array_t*	arr)		/* in/out: wait array */
{
	ulint		i = 0;
	ulint		count;

	sync_array_enter(arr);

	for (count = 0;  count < arr->n_reserved; ++i) {
		sync_cell_t*	cell;

		cell = sync_array_get_nth_cell(arr, i);

		if (cell->wait_object != NULL) {

			count++;

			if (sync_arr_cell_can_wake_up(cell)) {
				os_event_t      event;

				event = sync_cell_get_event(cell);

				os_event_set(event);
			}
		}
	}

	sync_array_exit(arr);
}

/**********************************************************************//**
If the wakeup algorithm does not work perfectly at semaphore relases,
this function will do the waking (see the comment in mutex_exit). This
function should be called about every 1 second in the server.

Note that there's a race condition between this thread and mutex_exit
changing the lock_word and calling signal_object, so sometimes this finds
threads to wake up even when nothing has gone wrong. */
UNIV_INTERN
void
sync_arr_wake_threads_if_sema_free(void)
/*====================================*/
{
	ulint		i;

	for (i = 0; i < sync_array_size; ++i) {

		sync_array_wake_threads_if_sema_free_low(
			sync_wait_array[i]);
	}
}

/**********************************************************************//**
Prints warnings of long semaphore waits to stderr.
@return	TRUE if fatal semaphore wait threshold was exceeded */
static
ibool
sync_array_print_long_waits_low(
/*============================*/
	sync_array_t*	arr,	/*!< in: sync array instance */
	os_thread_id_t*	waiter,	/*!< out: longest waiting thread */
	const void**	sema,	/*!< out: longest-waited-for semaphore */
	ibool*		noticed)/*!< out: TRUE if long wait noticed */
{
	ulint		i;
	ulint		fatal_timeout = srv_fatal_semaphore_wait_threshold;
	ibool		fatal = FALSE;
	double		longest_diff = 0;

	/* For huge tables, skip the check during CHECK TABLE etc... */
	if (fatal_timeout > SRV_SEMAPHORE_WAIT_EXTENSION) {
		return(FALSE);
	}

#ifdef UNIV_DEBUG_VALGRIND
	/* Increase the timeouts if running under valgrind because it executes
	extremely slowly. UNIV_DEBUG_VALGRIND does not necessary mean that
	we are running under valgrind but we have no better way to tell.
	See Bug#58432 innodb.innodb_bug56143 fails under valgrind
	for an example */
# define SYNC_ARRAY_TIMEOUT	2400
	fatal_timeout *= 10;
#else
# define SYNC_ARRAY_TIMEOUT	240
#endif

	for (i = 0; i < arr->n_cells; i++) {

		double		diff;
		sync_cell_t*	cell;
		void*		wait_object;

		cell = sync_array_get_nth_cell(arr, i);

		wait_object = cell->wait_object;

		if (wait_object == NULL || !cell->waiting) {

			continue;
		}

		diff = difftime(time(NULL), cell->reservation_time);

		if (diff > SYNC_ARRAY_TIMEOUT) {
			fputs("InnoDB: Warning: a long semaphore wait:\n",
			      stderr);
			sync_array_cell_print(stderr, cell);
			*noticed = TRUE;
		}

		if (diff > fatal_timeout) {
			fatal = TRUE;
		}

		if (diff > longest_diff) {
			longest_diff = diff;
			*sema = wait_object;
			*waiter = cell->thread;
		}
	}

#undef SYNC_ARRAY_TIMEOUT

	return(fatal);
}

/**********************************************************************//**
Prints warnings of long semaphore waits to stderr.
@return	TRUE if fatal semaphore wait threshold was exceeded */
UNIV_INTERN
ibool
sync_array_print_long_waits(
/*========================*/
	os_thread_id_t*	waiter,	/*!< out: longest waiting thread */
	const void**	sema)	/*!< out: longest-waited-for semaphore */
{
	ulint		i;
	ibool		fatal = FALSE;
	ibool		noticed = FALSE;

	for (i = 0; i < sync_array_size; ++i) {

		sync_array_t*	arr = sync_wait_array[i];

		sync_array_enter(arr);

		if (sync_array_print_long_waits_low(
				arr, waiter, sema, &noticed)) {

			fatal = TRUE;
		}

		sync_array_exit(arr);
	}

	if (noticed) {
		ibool	old_val;

		fprintf(stderr,
			"InnoDB: ###### Starts InnoDB Monitor"
			" for 30 secs to print diagnostic info:\n");

		old_val = srv_print_innodb_monitor;

		/* If some crucial semaphore is reserved, then also the InnoDB
		Monitor can hang, and we do not get diagnostics. Since in
		many cases an InnoDB hang is caused by a pwrite() or a pread()
		call hanging inside the operating system, let us print right
		now the values of pending calls of these. */

		fprintf(stderr,
			"InnoDB: Pending preads %lu, pwrites %lu\n",
			(ulong) os_file_n_pending_preads,
			(ulong) os_file_n_pending_pwrites);

		srv_print_innodb_monitor = TRUE;
		os_event_set(lock_sys->timeout_event);

		os_thread_sleep(30000000);

		srv_print_innodb_monitor = old_val;
		fprintf(stderr,
			"InnoDB: ###### Diagnostic info printed"
			" to the standard error stream\n");
	}

	return(fatal);
}

/**********************************************************************//**
Prints info of the wait array. */
static
void
sync_array_print_info_low(
/*======================*/
	FILE*		file,	/*!< in: file where to print */
	sync_array_t*	arr)	/*!< in: wait array */
{
	ulint		i;
	ulint		count = 0;

	fprintf(file,
		"OS WAIT ARRAY INFO: reservation count %ld\n",
		(long) arr->res_count);

	for (i = 0; count < arr->n_reserved; ++i) {
		sync_cell_t*	cell;

		cell = sync_array_get_nth_cell(arr, i);

		if (cell->wait_object != NULL) {
			count++;
			sync_array_cell_print(file, cell);
		}
	}
}

/**********************************************************************//**
Prints info of the wait array. */
static
void
sync_array_print_info(
/*==================*/
	FILE*		file,	/*!< in: file where to print */
	sync_array_t*	arr)	/*!< in: wait array */
{
	sync_array_enter(arr);

	sync_array_print_info_low(file, arr);

	sync_array_exit(arr);
}

/**********************************************************************//**
Create the primary system wait array(s), they are protected by an OS mutex */
UNIV_INTERN
void
sync_array_init(
/*============*/
	ulint		n_threads)		/*!< in: Number of slots to
						create in all arrays */
{
	ulint		i;
	ulint		n_slots;

	ut_a(sync_wait_array == NULL);
	ut_a(srv_sync_array_size > 0);
	ut_a(n_threads > 0);

	sync_array_size = srv_sync_array_size;

	/* We have to use ut_malloc() because the mutex infrastructure
	hasn't been initialised yet. It is required by mem_alloc() and
	the heap functions. */

	sync_wait_array = static_cast<sync_array_t**>(
		ut_malloc(sizeof(*sync_wait_array) * sync_array_size));

	n_slots = 1 + (n_threads - 1) / sync_array_size;

	for (i = 0; i < sync_array_size; ++i) {

		sync_wait_array[i] = sync_array_create(n_slots);
	}
}

/**********************************************************************//**
Close sync array wait sub-system. */
UNIV_INTERN
void
sync_array_close(void)
/*==================*/
{
	ulint		i;

	for (i = 0; i < sync_array_size; ++i) {
		sync_array_free(sync_wait_array[i]);
	}

	ut_free(sync_wait_array);
	sync_wait_array = NULL;
}

/**********************************************************************//**
Print info about the sync array(s). */
UNIV_INTERN
void
sync_array_print(
/*=============*/
	FILE*		file)		/*!< in/out: Print to this stream */
{
	ulint		i;

	for (i = 0; i < sync_array_size; ++i) {
		sync_array_print_info(file, sync_wait_array[i]);
	}

	fprintf(file,
		"OS WAIT ARRAY INFO: signal count %ld\n", (long) sg_count);

}

/**********************************************************************//**
Get an instance of the sync wait array. */
UNIV_INTERN
sync_array_t*
sync_array_get(void)
/*================*/
{
	ulint		i;
	static ulint	count;

#ifdef HAVE_ATOMIC_BUILTINS
	i = os_atomic_increment_ulint(&count, 1);
#else
	i = count++;
#endif /* HAVE_ATOMIC_BUILTINS */

	return(sync_wait_array[i % sync_array_size]);
}
