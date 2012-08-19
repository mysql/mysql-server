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

reated 9/5/1995 Heikki Tuuri
*******************************************************/

#include "sync0sync.h"
#ifdef UNIV_NONINL
#include "sync0sync.ic"
#endif

#include "sync0rw.h"
#include "buf0buf.h"
#include "srv0srv.h"
#include "buf0types.h"
#include "os0sync.h"
#include "os0thread.h"
#ifdef UNIV_SYNC_DEBUG
# include "srv0start.h"
#endif /* UNIV_SYNC_DEBUG */
#include "ha_prototypes.h"

// Forward declaration
struct sync_level_t;
struct sync_thread_t;

/** This variable is set to TRUE when sync_init is called */
UNIV_INTERN bool		sync_initialized;

#ifdef UNIV_SYNC_DEBUG
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
UNIV_INTERN bool		sync_order_checks_on;

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
struct sync_thread_t {
	os_thread_id_t	id;		/*!< OS thread id */
	sync_arr_t*	levels;		/*!< level array for this thread; if
					this is NULL this slot is unused */
};

/** An acquired mutex or rw-lock and its level in the latching order */
struct sync_level_t {
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
};

/******************************************************************//**
Looks for the thread slot for the calling thread.
@return	pointer to thread slot, NULL if not found */
static
sync_thread_t*
sync_thread_level_arrays_find_slot()
/*================================*/

{
	os_thread_id_t	id;

	id = os_thread_get_curr_id();

	for (ulint i = 0; i < OS_THREAD_MAX_N; i++) {
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
sync_thread_level_arrays_find_free()
/*================================*/

{
	for (ulint i = 0; i < OS_THREAD_MAX_N; i++) {
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
	ib_mutex_t*	mutex = static_cast<ib_mutex_t*>(slot->latch);

	if (mutex->magic_n == MUTEX_MAGIC_N) {
		ib_logf(IB_LOG_LEVEL_WARN,
			"Mutex created at %s %lu",
			innobase_basename(mutex->cfile_name),
			(ulong) mutex->cline);

		if (mutex_get_lock_word(mutex) != 0) {
			ulint		line;
			const char*	file_name;
			os_thread_id_t	thread_id;

			mutex_get_debug_info(
				mutex, &file_name, &line, &thread_id);

			ib_logf(IB_LOG_LEVEL_WARN,
				"Locked mutex:"
				" addr %p thread %ld file %s line %ld",
				(void*) mutex, os_thread_pf(thread_id),
				file_name, (ulong) line);
		} else {
			ib_logf(IB_LOG_LEVEL_WARN,"Not locked");
		}
	} else {
		rw_lock_t*	lock = static_cast<rw_lock_t*>(slot->latch);

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
	for (ulint i = 0; i < arr->n_elems; i++) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL && slot->level <= limit) {
			if (warn) {
				ib_logf(IB_LOG_LEVEL_WARN,
					"Sync levels should be"
					" > %lu but a level is %lu",
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
	for (ulint i = 0; i < arr->n_elems; i++) {
		const sync_level_t*	slot;

		slot = &arr->elems[i];

		if (slot->latch != NULL && slot->level == level) {

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

	for (ulint i = 0; i < arr->n_elems; i++) {
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

	for (ulint i = 0; i < arr->n_elems; ++i) {
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

	for (ulint i = 0; i < arr->n_elems; ++i) {
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
	ulint	i;

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

	rw_lock_debug_event = os_event_create(NULL);
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
		(ib_uint64_t) mutex_spin_wait_count_get(),
		(ib_uint64_t) mutex_spin_round_count_get(),
		(ib_uint64_t) mutex_os_wait_count_get(),
		(ib_uint64_t) rw_lock_stats.rw_s_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_s_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_x_os_wait_count);

	fprintf(file,
		"Spin rounds per wait: %.2f mutex, %.2f RW-shared, "
		"%.2f RW-excl\n",
		(double) mutex_spin_round_count_get() /
		(mutex_spin_wait_count_get() ? mutex_spin_wait_count_get() : 1),
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
