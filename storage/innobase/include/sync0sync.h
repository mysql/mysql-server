/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2012, Facebook Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/sync0sync.h
 Mutex, the basic synchronization primitive

 Created 9/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef sync0sync_h
#define sync0sync_h

#include "univ.i"
#include "ut0counter.h"

#ifdef HAVE_PSI_INTERFACE

/** Define for performance schema registration key */
struct mysql_pfs_key_t {
 public:
  /** Default Constructor */
  mysql_pfs_key_t() { s_count++; }

  /** Constructor */
  mysql_pfs_key_t(unsigned int val) : m_value(val) {}

  /** Retreive the count.
  @return number of keys defined */
  static int get_count() { return s_count; }

  /* Key value. */
  unsigned int m_value;

 private:
  /** To keep count of number of PS keys defined. */
  static unsigned int s_count;
};

#endif /* HAVE_PFS_INTERFACE */

#if defined UNIV_PFS_MUTEX || defined UNIV_PFS_RWLOCK

/* By default, buffer mutexes and rwlocks will be excluded from
instrumentation due to their large number of instances. */
#define PFS_SKIP_BUFFER_MUTEX_RWLOCK

/* By default, event->mutex will also be excluded from instrumentation */
#define PFS_SKIP_EVENT_MUTEX

#endif /* UNIV_PFS_MUTEX || UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
/* Key defines to register InnoDB mutexes with performance schema */
extern mysql_pfs_key_t autoinc_mutex_key;
extern mysql_pfs_key_t autoinc_persisted_mutex_key;
#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
extern mysql_pfs_key_t buffer_block_mutex_key;
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
extern mysql_pfs_key_t buf_pool_flush_state_mutex_key;
extern mysql_pfs_key_t buf_pool_LRU_list_mutex_key;
extern mysql_pfs_key_t buf_pool_free_list_mutex_key;
extern mysql_pfs_key_t buf_pool_zip_free_mutex_key;
extern mysql_pfs_key_t buf_pool_zip_hash_mutex_key;
extern mysql_pfs_key_t buf_pool_zip_mutex_key;
extern mysql_pfs_key_t cache_last_read_mutex_key;
extern mysql_pfs_key_t dict_foreign_err_mutex_key;
extern mysql_pfs_key_t dict_persist_dirty_tables_mutex_key;
extern mysql_pfs_key_t dict_sys_mutex_key;
extern mysql_pfs_key_t dict_table_mutex_key;
extern mysql_pfs_key_t parser_mutex_key;
extern mysql_pfs_key_t fil_system_mutex_key;
extern mysql_pfs_key_t flush_list_mutex_key;
extern mysql_pfs_key_t fts_bg_threads_mutex_key;
extern mysql_pfs_key_t fts_delete_mutex_key;
extern mysql_pfs_key_t fts_optimize_mutex_key;
extern mysql_pfs_key_t fts_doc_id_mutex_key;
extern mysql_pfs_key_t fts_pll_tokenize_mutex_key;
extern mysql_pfs_key_t hash_table_mutex_key;
extern mysql_pfs_key_t ibuf_bitmap_mutex_key;
extern mysql_pfs_key_t ibuf_mutex_key;
extern mysql_pfs_key_t ibuf_pessimistic_insert_mutex_key;
extern mysql_pfs_key_t lock_free_hash_mutex_key;
extern mysql_pfs_key_t log_checkpointer_mutex_key;
extern mysql_pfs_key_t log_closer_mutex_key;
extern mysql_pfs_key_t log_writer_mutex_key;
extern mysql_pfs_key_t log_flusher_mutex_key;
extern mysql_pfs_key_t log_write_notifier_mutex_key;
extern mysql_pfs_key_t log_flush_notifier_mutex_key;
extern mysql_pfs_key_t log_cmdq_mutex_key;
extern mysql_pfs_key_t log_sn_lock_key;
extern mysql_pfs_key_t log_sys_arch_mutex_key;
extern mysql_pfs_key_t page_sys_arch_mutex_key;
extern mysql_pfs_key_t page_sys_arch_oper_mutex_key;
extern mysql_pfs_key_t mutex_list_mutex_key;
extern mysql_pfs_key_t recalc_pool_mutex_key;
extern mysql_pfs_key_t page_cleaner_mutex_key;
extern mysql_pfs_key_t purge_sys_pq_mutex_key;
extern mysql_pfs_key_t recv_sys_mutex_key;
extern mysql_pfs_key_t recv_writer_mutex_key;
extern mysql_pfs_key_t rtr_active_mutex_key;
extern mysql_pfs_key_t rtr_match_mutex_key;
extern mysql_pfs_key_t rtr_path_mutex_key;
extern mysql_pfs_key_t rtr_ssn_mutex_key;
extern mysql_pfs_key_t temp_space_rseg_mutex_key;
extern mysql_pfs_key_t undo_space_rseg_mutex_key;
extern mysql_pfs_key_t trx_sys_rseg_mutex_key;
extern mysql_pfs_key_t page_zip_stat_per_index_mutex_key;
#ifdef UNIV_DEBUG
extern mysql_pfs_key_t rw_lock_debug_mutex_key;
#endif /* UNIV_DEBUG */
extern mysql_pfs_key_t rw_lock_list_mutex_key;
extern mysql_pfs_key_t rw_lock_mutex_key;
extern mysql_pfs_key_t srv_dict_tmpfile_mutex_key;
extern mysql_pfs_key_t srv_innodb_monitor_mutex_key;
extern mysql_pfs_key_t srv_misc_tmpfile_mutex_key;
extern mysql_pfs_key_t srv_monitor_file_mutex_key;
#ifdef UNIV_DEBUG
extern mysql_pfs_key_t sync_thread_mutex_key;
#endif /* UNIV_DEBUG */
extern mysql_pfs_key_t buf_dblwr_mutex_key;
extern mysql_pfs_key_t trx_undo_mutex_key;
extern mysql_pfs_key_t trx_mutex_key;
extern mysql_pfs_key_t trx_pool_mutex_key;
extern mysql_pfs_key_t trx_pool_manager_mutex_key;
extern mysql_pfs_key_t temp_pool_manager_mutex_key;
extern mysql_pfs_key_t lock_mutex_key;
extern mysql_pfs_key_t lock_wait_mutex_key;
extern mysql_pfs_key_t trx_sys_mutex_key;
extern mysql_pfs_key_t srv_sys_mutex_key;
extern mysql_pfs_key_t srv_threads_mutex_key;
#ifndef PFS_SKIP_EVENT_MUTEX
extern mysql_pfs_key_t event_mutex_key;
extern mysql_pfs_key_t event_manager_mutex_key;
#endif /* !PFS_SKIP_EVENT_MUTEX */
extern mysql_pfs_key_t sync_array_mutex_key;
extern mysql_pfs_key_t thread_mutex_key;
extern mysql_pfs_key_t zip_pad_mutex_key;
extern mysql_pfs_key_t row_drop_list_mutex_key;
extern mysql_pfs_key_t file_open_mutex_key;
extern mysql_pfs_key_t master_key_id_mutex_key;
extern mysql_pfs_key_t clone_sys_mutex_key;
extern mysql_pfs_key_t clone_task_mutex_key;
extern mysql_pfs_key_t clone_snapshot_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_PFS_RWLOCK
/* Following are rwlock keys used to register with MySQL
performance schema */
extern mysql_pfs_key_t btr_search_latch_key;
#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
extern mysql_pfs_key_t buf_block_lock_key;
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
#ifdef UNIV_DEBUG
extern mysql_pfs_key_t buf_block_debug_latch_key;
#endif /* UNIV_DEBUG */
extern mysql_pfs_key_t dict_operation_lock_key;
extern mysql_pfs_key_t undo_spaces_lock_key;
extern mysql_pfs_key_t rsegs_lock_key;
extern mysql_pfs_key_t fil_space_latch_key;
extern mysql_pfs_key_t fts_cache_rw_lock_key;
extern mysql_pfs_key_t fts_cache_init_rw_lock_key;
extern mysql_pfs_key_t trx_i_s_cache_lock_key;
extern mysql_pfs_key_t trx_purge_latch_key;
extern mysql_pfs_key_t index_tree_rw_lock_key;
extern mysql_pfs_key_t index_online_log_key;
extern mysql_pfs_key_t dict_table_stats_key;
extern mysql_pfs_key_t trx_sys_rw_lock_key;
extern mysql_pfs_key_t hash_table_locks_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef HAVE_PSI_INTERFACE
/* There are mutexes/rwlocks that we want to exclude from instrumentation
even if their corresponding performance schema define is set. And this
PFS_NOT_INSTRUMENTED is used as the key value to identify those objects that
would be excluded from instrumentation.*/
extern mysql_pfs_key_t PFS_NOT_INSTRUMENTED;
#endif /* HAVE_PFS_INTERFACE */

/** Prints info of the sync system.
@param[in]	file	where to print */
void sync_print(FILE *file);

#endif /* !sync0sync_h */
