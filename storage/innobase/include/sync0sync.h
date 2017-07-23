/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.
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
# ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
extern mysql_pfs_key_t	buffer_block_mutex_key;
# endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK */
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
extern mysql_pfs_key_t	fts_pll_tokenize_mutex_key;
extern mysql_pfs_key_t	hash_table_mutex_key;
extern mysql_pfs_key_t	ibuf_bitmap_mutex_key;
extern mysql_pfs_key_t	ibuf_mutex_key;
extern mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
extern mysql_pfs_key_t	log_sys_mutex_key;
extern mysql_pfs_key_t	log_sys_write_mutex_key;
extern mysql_pfs_key_t	log_cmdq_mutex_key;
extern mysql_pfs_key_t	log_flush_order_mutex_key;
extern mysql_pfs_key_t	mutex_list_mutex_key;
extern mysql_pfs_key_t	recalc_pool_mutex_key;
extern mysql_pfs_key_t	page_cleaner_mutex_key;
extern mysql_pfs_key_t	purge_sys_pq_mutex_key;
extern mysql_pfs_key_t	recv_sys_mutex_key;
extern mysql_pfs_key_t	recv_writer_mutex_key;
extern mysql_pfs_key_t	rtr_active_mutex_key;
extern mysql_pfs_key_t	rtr_match_mutex_key;
extern mysql_pfs_key_t	rtr_path_mutex_key;
extern mysql_pfs_key_t	rtr_ssn_mutex_key;
extern mysql_pfs_key_t	redo_rseg_mutex_key;
extern mysql_pfs_key_t	noredo_rseg_mutex_key;
extern mysql_pfs_key_t page_zip_stat_per_index_mutex_key;
# ifdef UNIV_DEBUG
extern mysql_pfs_key_t	rw_lock_debug_mutex_key;
# endif /* UNIV_DEBUG */
extern mysql_pfs_key_t	rw_lock_list_mutex_key;
extern mysql_pfs_key_t	rw_lock_mutex_key;
extern mysql_pfs_key_t	srv_dict_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_innodb_monitor_mutex_key;
extern mysql_pfs_key_t	srv_misc_tmpfile_mutex_key;
extern mysql_pfs_key_t	srv_monitor_file_mutex_key;
# ifdef UNIV_DEBUG
extern mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_DEBUG */
extern mysql_pfs_key_t	buf_dblwr_mutex_key;
extern mysql_pfs_key_t	trx_undo_mutex_key;
extern mysql_pfs_key_t	trx_mutex_key;
extern mysql_pfs_key_t	trx_pool_mutex_key;
extern mysql_pfs_key_t	trx_pool_manager_mutex_key;
extern mysql_pfs_key_t	lock_mutex_key;
extern mysql_pfs_key_t	lock_wait_mutex_key;
extern mysql_pfs_key_t	trx_sys_mutex_key;
extern mysql_pfs_key_t	srv_sys_mutex_key;
extern mysql_pfs_key_t	srv_threads_mutex_key;
# ifndef PFS_SKIP_EVENT_MUTEX
extern mysql_pfs_key_t	event_mutex_key;
extern mysql_pfs_key_t	event_manager_mutex_key;
# endif /* PFS_SKIP_EVENT_MUTEX */
extern mysql_pfs_key_t	sync_array_mutex_key;
extern mysql_pfs_key_t	thread_mutex_key;
extern mysql_pfs_key_t  zip_pad_mutex_key;
extern mysql_pfs_key_t  row_drop_list_mutex_key;
extern mysql_pfs_key_t	master_key_id_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_PFS_RWLOCK
/* Following are rwlock keys used to register with MySQL
performance schema */
extern	mysql_pfs_key_t btr_search_latch_key;
# ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
extern	mysql_pfs_key_t	buf_block_lock_key;
# endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK */
# ifdef UNIV_DEBUG
extern	mysql_pfs_key_t	buf_block_debug_latch_key;
# endif /* UNIV_DEBUG */
extern	mysql_pfs_key_t	dict_operation_lock_key;
extern	mysql_pfs_key_t	checkpoint_lock_key;
extern	mysql_pfs_key_t	fil_space_latch_key;
extern	mysql_pfs_key_t	fts_cache_rw_lock_key;
extern	mysql_pfs_key_t	fts_cache_init_rw_lock_key;
extern	mysql_pfs_key_t	trx_i_s_cache_lock_key;
extern	mysql_pfs_key_t	trx_purge_latch_key;
extern	mysql_pfs_key_t	index_tree_rw_lock_key;
extern	mysql_pfs_key_t	index_online_log_key;
extern	mysql_pfs_key_t	dict_table_stats_key;
extern  mysql_pfs_key_t trx_sys_rw_lock_key;
extern  mysql_pfs_key_t hash_table_locks_key;
#endif /* UNIV_PFS_RWLOCK */

/* There are mutexes/rwlocks that we want to exclude from instrumentation
even if their corresponding performance schema define is set. And this
PFS_NOT_INSTRUMENTED is used as the key value to identify those objects that
would be excluded from instrumentation.*/
extern mysql_pfs_key_t	PFS_NOT_INSTRUMENTED;

/** Prints info of the sync system.
@param[in]	file	where to print */
void
sync_print(FILE* file);

#endif /* !sync0sync_h */
