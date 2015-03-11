/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All Rights Reserved.
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

#include "univ.i"
#include "sync0rw.h"
#include "sync0sync.h"

#ifdef UNIV_PFS_MUTEX
/* Key to register autoinc_mutex with performance schema */
mysql_pfs_key_t	autoinc_mutex_key;
mysql_pfs_key_t	buffer_block_mutex_key;
mysql_pfs_key_t	buf_pool_mutex_key;
mysql_pfs_key_t	buf_pool_zip_mutex_key;
mysql_pfs_key_t	cache_last_read_mutex_key;
mysql_pfs_key_t	dict_foreign_err_mutex_key;
mysql_pfs_key_t	dict_sys_mutex_key;
mysql_pfs_key_t	fil_system_mutex_key;
mysql_pfs_key_t	flush_list_mutex_key;
mysql_pfs_key_t	fts_bg_threads_mutex_key;
mysql_pfs_key_t	fts_delete_mutex_key;
mysql_pfs_key_t	fts_optimize_mutex_key;
mysql_pfs_key_t	fts_doc_id_mutex_key;
mysql_pfs_key_t	fts_pll_tokenize_mutex_key;
mysql_pfs_key_t	hash_table_mutex_key;
mysql_pfs_key_t	ibuf_bitmap_mutex_key;
mysql_pfs_key_t	ibuf_mutex_key;
mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
mysql_pfs_key_t	log_sys_mutex_key;
mysql_pfs_key_t	log_cmdq_mutex_key;
mysql_pfs_key_t	log_flush_order_mutex_key;
mysql_pfs_key_t	recalc_pool_mutex_key;
mysql_pfs_key_t	page_cleaner_mutex_key;
mysql_pfs_key_t	purge_sys_pq_mutex_key;
mysql_pfs_key_t	recv_sys_mutex_key;
mysql_pfs_key_t	recv_writer_mutex_key;
mysql_pfs_key_t	redo_rseg_mutex_key;
mysql_pfs_key_t	noredo_rseg_mutex_key;
mysql_pfs_key_t page_zip_stat_per_index_mutex_key;
# ifdef UNIV_SYNC_DEBUG
mysql_pfs_key_t	rw_lock_debug_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
mysql_pfs_key_t rtr_active_mutex_key;
mysql_pfs_key_t	rtr_match_mutex_key;
mysql_pfs_key_t	rtr_path_mutex_key;
mysql_pfs_key_t rtr_ssn_mutex_key;
mysql_pfs_key_t	rw_lock_list_mutex_key;
mysql_pfs_key_t	rw_lock_mutex_key;
mysql_pfs_key_t	srv_dict_tmpfile_mutex_key;
mysql_pfs_key_t	srv_innodb_monitor_mutex_key;
mysql_pfs_key_t	srv_misc_tmpfile_mutex_key;
mysql_pfs_key_t	srv_monitor_file_mutex_key;
# ifdef UNIV_SYNC_DEBUG
mysql_pfs_key_t	sync_thread_mutex_key;
# endif /* UNIV_SYNC_DEBUG */
mysql_pfs_key_t	buf_dblwr_mutex_key;
mysql_pfs_key_t	trx_undo_mutex_key;
mysql_pfs_key_t	trx_mutex_key;
mysql_pfs_key_t	trx_pool_mutex_key;
mysql_pfs_key_t	trx_pool_manager_mutex_key;
mysql_pfs_key_t	lock_mutex_key;
mysql_pfs_key_t	lock_wait_mutex_key;
mysql_pfs_key_t	trx_sys_mutex_key;
mysql_pfs_key_t	srv_sys_mutex_key;
mysql_pfs_key_t	srv_threads_mutex_key;
mysql_pfs_key_t	event_mutex_key;
mysql_pfs_key_t	event_manager_mutex_key;
mysql_pfs_key_t	sync_array_mutex_key;
mysql_pfs_key_t	thread_mutex_key;
mysql_pfs_key_t zip_pad_mutex_key;
mysql_pfs_key_t row_drop_list_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_PFS_RWLOCK
mysql_pfs_key_t	btr_search_latch_key;
mysql_pfs_key_t	buf_block_lock_key;
# ifdef UNIV_SYNC_DEBUG
mysql_pfs_key_t	buf_block_debug_latch_key;
# endif /* UNIV_SYNC_DEBUG */
mysql_pfs_key_t	checkpoint_lock_key;
mysql_pfs_key_t	dict_operation_lock_key;
mysql_pfs_key_t	dict_table_stats_key;
mysql_pfs_key_t	hash_table_locks_key;
mysql_pfs_key_t	index_tree_rw_lock_key;
mysql_pfs_key_t	index_online_log_key;
mysql_pfs_key_t	fil_space_latch_key;
mysql_pfs_key_t	fts_cache_rw_lock_key;
mysql_pfs_key_t	fts_cache_init_rw_lock_key;
mysql_pfs_key_t trx_i_s_cache_lock_key;
mysql_pfs_key_t	trx_purge_latch_key;
# ifdef UNIV_DEBUG
mysql_pfs_key_t	buf_chunk_map_latch_key;
# endif /* UNIV_DEBUG */
#endif /* UNIV_PFS_RWLOCK */

/** The number of iterations in the mutex_spin_wait() spin loop.
Intended for performance monitoring. */
mutex_counter_t	mutex_spin_round_count;

/** The number of mutex_spin_wait() calls.  Intended for
performance monitoring. */
mutex_counter_t	mutex_spin_wait_count;

/** The number of OS waits in mutex_spin_wait().  Intended for
performance monitoring. */
mutex_counter_t	mutex_os_wait_count;

/**
Prints wait info of the sync system.
@param file - where to print */
static
void
sync_print_wait_info(FILE* file)
{
	fprintf(file,
		"Mutex spin waits " UINT64PF ", rounds " UINT64PF ","
		" OS waits " UINT64PF "\n"
		"RW-shared spins " UINT64PF ", rounds " UINT64PF ","
		" OS waits " UINT64PF "\n"
		"RW-excl spins " UINT64PF ", rounds " UINT64PF ","
		" OS waits " UINT64PF "\n"
		"RW-sx spins " UINT64PF ", rounds " UINT64PF ","
		" OS waits " UINT64PF "\n",
		(ib_uint64_t) mutex_spin_wait_count,
		(ib_uint64_t) mutex_spin_round_count,
		(ib_uint64_t) mutex_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_s_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_x_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_sx_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_sx_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_sx_os_wait_count);

	fprintf(file,
		"Spin rounds per wait: %.2f mutex, %.2f RW-shared,"
		" %.2f RW-excl, %.2f RW-sx\n",
		(double) mutex_spin_round_count /
		(mutex_spin_wait_count_get() ? mutex_spin_wait_count_get() : 1),
		(double) rw_lock_stats.rw_s_spin_round_count /
		(rw_lock_stats.rw_s_spin_wait_count
		 ? rw_lock_stats.rw_s_spin_wait_count : 1),
		(double) rw_lock_stats.rw_x_spin_round_count /
		(rw_lock_stats.rw_x_spin_wait_count
		 ? rw_lock_stats.rw_x_spin_wait_count : 1),
		(double) rw_lock_stats.rw_sx_spin_round_count /
		(rw_lock_stats.rw_sx_spin_wait_count
		 ? rw_lock_stats.rw_sx_spin_wait_count : 1));
}

/**
Prints info of the sync system.
@param file - where to print */
void
sync_print(FILE* file)
{
#ifdef UNIV_SYNC_DEBUG
	rw_lock_list_print_info(file);
#endif /* UNIV_SYNC_DEBUG */

	sync_array_print(file);

	sync_print_wait_info(file);
}

/**
@return total number of spin rounds since startup. */
ib_uint64_t
mutex_spin_round_count_get()
{
	return(mutex_spin_round_count);
}

/**
@return total number of spin wait calls since startup. */
ib_uint64_t
mutex_spin_wait_count_get()
{
	return(mutex_spin_wait_count);
}

/**
@return total number of OS waits since startup. */
ib_uint64_t
mutex_os_wait_count_get()
{
	return(mutex_os_wait_count);
}
