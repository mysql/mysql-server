/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2012, Facebook Inc.

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
@file include/sync0sync.h
Mutex, the basic synchronization primitive

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0sync_h
#define sync0sync_h

#include "univ.i"
#include "ut0counter.h"

#if defined UNIV_PFS_MUTEX || defined UNIV_PFS_RWLOCK

/* By default, buffer mutexes and rwlocks will be excluded from
instrumentation due to their large number of instances. */
# define PFS_SKIP_BUFFER_MUTEX_RWLOCK

/* By default, event->mutex will also be excluded from instrumentation */
# define PFS_SKIP_EVENT_MUTEX

#endif /* UNIV_PFS_MUTEX || UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
/* Key defines to register InnoDB mutexes with performance schema */
extern mysql_pfs_key_t	autoinc_mutex_key;
extern mysql_pfs_key_t	buffer_block_mutex_key;
extern mysql_pfs_key_t	buf_pool_mutex_key;
extern mysql_pfs_key_t	buf_pool_zip_mutex_key;
extern mysql_pfs_key_t	cache_last_read_mutex_key;
extern mysql_pfs_key_t	dict_foreign_err_mutex_key;
extern mysql_pfs_key_t	dict_sys_mutex_key;
extern mysql_pfs_key_t	file_format_max_mutex_key;
extern mysql_pfs_key_t	fil_system_mutex_key;
extern mysql_pfs_key_t	flush_list_mutex_key;
extern mysql_pfs_key_t	fts_bg_threads_mutex_key;
extern mysql_pfs_key_t	fts_delete_mutex_key;
extern mysql_pfs_key_t	fts_optimize_mutex_key;
extern mysql_pfs_key_t	fts_doc_id_mutex_key;
extern mysql_pfs_key_t	hash_table_mutex_key;
extern mysql_pfs_key_t	ibuf_bitmap_mutex_key;
extern mysql_pfs_key_t	ibuf_mutex_key;
extern mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
extern mysql_pfs_key_t	log_sys_mutex_key;
extern mysql_pfs_key_t	log_flush_order_mutex_key;
# ifndef HAVE_ATOMIC_BUILTINS
extern mysql_pfs_key_t	server_mutex_key;
# endif /* !HAVE_ATOMIC_BUILTINS */
# ifdef UNIV_MEM_DEBUG
extern mysql_pfs_key_t	mem_hash_mutex_key;
# endif /* UNIV_MEM_DEBUG */
extern mysql_pfs_key_t	mem_pool_mutex_key;
extern mysql_pfs_key_t	purge_sys_bh_mutex_key;
extern mysql_pfs_key_t	recalc_pool_mutex_key;
extern mysql_pfs_key_t	recv_sys_mutex_key;
extern mysql_pfs_key_t	recv_writer_mutex_key;
extern mysql_pfs_key_t	rseg_mutex_key;
# ifdef UNIV_SYNC_DEBUG
extern mysql_pfs_key_t	rw_lock_debug_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
extern mysql_pfs_key_t	rw_lock_list_mutex_key;
extern mysql_pfs_key_t	rw_lock_mutex_key;
extern mysql_pfs_key_t	srv_dict_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_innodb_monitor_mutex_key;
extern mysql_pfs_key_t	srv_misc_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_monitor_file_mutex_key;
# ifdef UNIV_SYNC_DEBUG
extern mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
extern mysql_pfs_key_t	buf_dblwr_mutex_key;
extern mysql_pfs_key_t	trx_undo_mutex_key;
extern mysql_pfs_key_t	trx_mutex_key;
extern mysql_pfs_key_t	lock_sys_mutex_key;
extern mysql_pfs_key_t	lock_sys_wait_mutex_key;
extern mysql_pfs_key_t	trx_sys_mutex_key;
extern mysql_pfs_key_t	srv_sys_mutex_key;
extern mysql_pfs_key_t	srv_sys_tasks_mutex_key;
#ifndef HAVE_ATOMIC_BUILTINS
extern mysql_pfs_key_t	srv_conc_mutex_key;
#endif /* !HAVE_ATOMIC_BUILTINS */
#ifndef HAVE_ATOMIC_BUILTINS_64
extern mysql_pfs_key_t	monitor_mutex_key;
#endif /* !HAVE_ATOMIC_BUILTINS_64 */
extern mysql_pfs_key_t	event_mutex_key;
extern mysql_pfs_key_t	event_manager_mutex_key;
extern mysql_pfs_key_t	sync_array_mutex_key;
extern mysql_pfs_key_t	thread_mutex_key;
extern mysql_pfs_key_t	ut_list_mutex_key;
extern mysql_pfs_key_t  zip_pad_mutex_key;
extern mysql_pfs_key_t  row_drop_list_mutex_key;
#endif /* UNIV_PFS_MUTEX */

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
extern	mysql_pfs_key_t	checkpoint_lock_key;
extern	mysql_pfs_key_t	fil_space_latch_key;
extern	mysql_pfs_key_t	fts_cache_rw_lock_key;
extern	mysql_pfs_key_t	fts_cache_init_rw_lock_key;
extern	mysql_pfs_key_t	trx_i_s_cache_lock_key;
extern	mysql_pfs_key_t	trx_purge_latch_key;
extern	mysql_pfs_key_t	index_tree_rw_lock_key;
extern	mysql_pfs_key_t	index_online_log_key;
extern	mysql_pfs_key_t	dict_table_stats_latch_key;
extern  mysql_pfs_key_t trx_sys_rw_lock_key;
extern  mysql_pfs_key_t hash_table_rw_lock_key;
#endif /* UNIV_PFS_RWLOCK */

#ifndef HAVE_ATOMIC_BUILTINS

#include "sync0mutex.h"

/**********************************************************//**
Function that uses a mutex to decrement a variable atomically */
template <typename Mutex>
void
os_atomic_dec_ulint_func(
/*=====================*/
	Mutex*		mutex,		/*!< in: mutex guarding the dec */
	volatile ulint*	var,		/*!< in/out: variable to decrement */
	ulint		delta)		/*!< in: delta to decrement */
{
	mutex_enter(mutex);

	/* I don't think we will encounter a situation where
	this check will not be required. */

	ut_ad(*var >= delta);

	*var -= delta;

	mutex_exit(mutex);
}

/**********************************************************//**
Function that uses a mutex to increment a variable atomically */
template <typename Mutex>
void
os_atomic_inc_ulint_func(
/*=====================*/
	Mutex*		mutex,		/*!< in: mutex guarding the increment */
	volatile ulint*	var,		/*!< in/out: variable to increment */
	ulint		delta)		/*!< in: delta to increment */
{
	mutex_enter(mutex);

	*var += delta;

	mutex_exit(mutex);
}

#endif /* !HAVE_ATOMIC_BUILTINS */

/**
Prints info of the sync system. */
UNIV_INTERN
void
sync_print(
/*=======*/
	FILE*	file);				/*!< in/out: where to print */

/* Number of spin waits on mutexes: for performance monitoring */
typedef ib_counter_t<ib_int64_t, IB_N_SLOTS> mutex_counter_t;

/** The number of OS waits in mutex_spin_wait().  Intended for
performance monitoring. */
extern mutex_counter_t	mutex_os_wait_count;

/** The number of mutex_spin_wait() calls.  Intended for
performance monitoring. */
extern mutex_counter_t	mutex_spin_wait_count;

/** The number of iterations in the mutex_spin_wait() spin loop.
Intended for performance monitoring. */
extern mutex_counter_t	mutex_spin_round_count;

#endif /* !sync0sync_h */
