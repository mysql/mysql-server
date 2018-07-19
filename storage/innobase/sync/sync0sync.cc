/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

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

/** @file sync/sync0sync.cc
 Mutex, the basic synchronization primitive

 Created 9/5/1995 Heikki Tuuri
 *******************************************************/

#include "univ.i"

#include "sync0rw.h"
#include "sync0sync.h"

#ifdef HAVE_PSI_INTERFACE
/** To keep count of number of PS keys defined. */
unsigned int mysql_pfs_key_t::s_count = 0;
#endif /* HAVE_PSI_INTERFACE */

#ifdef UNIV_PFS_MUTEX
/* Key to register autoinc_mutex with performance schema */
mysql_pfs_key_t autoinc_mutex_key;
mysql_pfs_key_t autoinc_persisted_mutex_key;
#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
mysql_pfs_key_t buffer_block_mutex_key;
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
mysql_pfs_key_t buf_pool_flush_state_mutex_key;
mysql_pfs_key_t buf_pool_LRU_list_mutex_key;
mysql_pfs_key_t buf_pool_free_list_mutex_key;
mysql_pfs_key_t buf_pool_zip_free_mutex_key;
mysql_pfs_key_t buf_pool_zip_hash_mutex_key;
mysql_pfs_key_t buf_pool_zip_mutex_key;
mysql_pfs_key_t cache_last_read_mutex_key;
mysql_pfs_key_t dict_foreign_err_mutex_key;
mysql_pfs_key_t dict_persist_dirty_tables_mutex_key;
mysql_pfs_key_t dict_sys_mutex_key;
mysql_pfs_key_t dict_table_mutex_key;
mysql_pfs_key_t parser_mutex_key;
mysql_pfs_key_t fil_system_mutex_key;
mysql_pfs_key_t flush_list_mutex_key;
mysql_pfs_key_t fts_bg_threads_mutex_key;
mysql_pfs_key_t fts_delete_mutex_key;
mysql_pfs_key_t fts_optimize_mutex_key;
mysql_pfs_key_t fts_doc_id_mutex_key;
mysql_pfs_key_t fts_pll_tokenize_mutex_key;
mysql_pfs_key_t hash_table_mutex_key;
mysql_pfs_key_t ibuf_bitmap_mutex_key;
mysql_pfs_key_t ibuf_mutex_key;
mysql_pfs_key_t ibuf_pessimistic_insert_mutex_key;
mysql_pfs_key_t lock_free_hash_mutex_key;
mysql_pfs_key_t log_checkpointer_mutex_key;
mysql_pfs_key_t log_closer_mutex_key;
mysql_pfs_key_t log_writer_mutex_key;
mysql_pfs_key_t log_flusher_mutex_key;
mysql_pfs_key_t log_write_notifier_mutex_key;
mysql_pfs_key_t log_flush_notifier_mutex_key;
mysql_pfs_key_t log_cmdq_mutex_key;
mysql_pfs_key_t log_sn_lock_key;
mysql_pfs_key_t log_sys_arch_mutex_key;
mysql_pfs_key_t page_sys_arch_mutex_key;
mysql_pfs_key_t page_sys_arch_oper_mutex_key;
mysql_pfs_key_t mutex_list_mutex_key;
mysql_pfs_key_t recalc_pool_mutex_key;
mysql_pfs_key_t page_cleaner_mutex_key;
mysql_pfs_key_t purge_sys_pq_mutex_key;
mysql_pfs_key_t recv_sys_mutex_key;
mysql_pfs_key_t recv_writer_mutex_key;
mysql_pfs_key_t temp_space_rseg_mutex_key;
mysql_pfs_key_t undo_space_rseg_mutex_key;
mysql_pfs_key_t trx_sys_rseg_mutex_key;
mysql_pfs_key_t page_zip_stat_per_index_mutex_key;
#ifdef UNIV_DEBUG
mysql_pfs_key_t rw_lock_debug_mutex_key;
#endif /* UNIV_DEBUG */
mysql_pfs_key_t rtr_active_mutex_key;
mysql_pfs_key_t rtr_match_mutex_key;
mysql_pfs_key_t rtr_path_mutex_key;
mysql_pfs_key_t rtr_ssn_mutex_key;
mysql_pfs_key_t rw_lock_list_mutex_key;
mysql_pfs_key_t rw_lock_mutex_key;
mysql_pfs_key_t srv_dict_tmpfile_mutex_key;
mysql_pfs_key_t srv_innodb_monitor_mutex_key;
mysql_pfs_key_t srv_misc_tmpfile_mutex_key;
mysql_pfs_key_t srv_monitor_file_mutex_key;
#ifdef UNIV_DEBUG
mysql_pfs_key_t sync_thread_mutex_key;
#endif /* UNIV_DEBUG */
mysql_pfs_key_t buf_dblwr_mutex_key;
mysql_pfs_key_t trx_undo_mutex_key;
mysql_pfs_key_t trx_mutex_key;
mysql_pfs_key_t trx_pool_mutex_key;
mysql_pfs_key_t trx_pool_manager_mutex_key;
mysql_pfs_key_t temp_pool_manager_mutex_key;
mysql_pfs_key_t lock_mutex_key;
mysql_pfs_key_t lock_wait_mutex_key;
mysql_pfs_key_t trx_sys_mutex_key;
mysql_pfs_key_t srv_sys_mutex_key;
mysql_pfs_key_t srv_threads_mutex_key;
#ifndef PFS_SKIP_EVENT_MUTEX
mysql_pfs_key_t event_mutex_key;
mysql_pfs_key_t event_manager_mutex_key;
#endif /* !PFS_SKIP_EVENT_MUTEX */
mysql_pfs_key_t sync_array_mutex_key;
mysql_pfs_key_t zip_pad_mutex_key;
mysql_pfs_key_t row_drop_list_mutex_key;
mysql_pfs_key_t file_open_mutex_key;
mysql_pfs_key_t master_key_id_mutex_key;
mysql_pfs_key_t clone_sys_mutex_key;
mysql_pfs_key_t clone_task_mutex_key;
mysql_pfs_key_t clone_snapshot_mutex_key;

#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_PFS_RWLOCK
mysql_pfs_key_t btr_search_latch_key;
#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
mysql_pfs_key_t buf_block_lock_key;
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
#ifdef UNIV_DEBUG
mysql_pfs_key_t buf_block_debug_latch_key;
#endif /* UNIV_DEBUG */
mysql_pfs_key_t undo_spaces_lock_key;
mysql_pfs_key_t rsegs_lock_key;
mysql_pfs_key_t dict_operation_lock_key;
mysql_pfs_key_t dict_table_stats_key;
mysql_pfs_key_t hash_table_locks_key;
mysql_pfs_key_t index_tree_rw_lock_key;
mysql_pfs_key_t index_online_log_key;
mysql_pfs_key_t fil_space_latch_key;
mysql_pfs_key_t fts_cache_rw_lock_key;
mysql_pfs_key_t fts_cache_init_rw_lock_key;
mysql_pfs_key_t trx_i_s_cache_lock_key;
mysql_pfs_key_t trx_purge_latch_key;
#endif /* UNIV_PFS_RWLOCK */

/* There are mutexes/rwlocks that we want to exclude from instrumentation
even if their corresponding performance schema define is set. And this
PFS_NOT_INSTRUMENTED is used as the key value to identify those objects that
would be excluded from instrumentation.*/
mysql_pfs_key_t PFS_NOT_INSTRUMENTED(UINT32_UNDEFINED);

/** For monitoring active mutexes */
MutexMonitor *mutex_monitor;

/**
Prints wait info of the sync system.
@param file - where to print */
static void sync_print_wait_info(FILE *file) {
  fprintf(file,
          "RW-shared spins " UINT64PF ", rounds " UINT64PF
          ","
          " OS waits " UINT64PF
          "\n"
          "RW-excl spins " UINT64PF ", rounds " UINT64PF
          ","
          " OS waits " UINT64PF
          "\n"
          "RW-sx spins " UINT64PF ", rounds " UINT64PF
          ","
          " OS waits " UINT64PF "\n",
          (uint64_t)rw_lock_stats.rw_s_spin_wait_count,
          (uint64_t)rw_lock_stats.rw_s_spin_round_count,
          (uint64_t)rw_lock_stats.rw_s_os_wait_count,
          (uint64_t)rw_lock_stats.rw_x_spin_wait_count,
          (uint64_t)rw_lock_stats.rw_x_spin_round_count,
          (uint64_t)rw_lock_stats.rw_x_os_wait_count,
          (uint64_t)rw_lock_stats.rw_sx_spin_wait_count,
          (uint64_t)rw_lock_stats.rw_sx_spin_round_count,
          (uint64_t)rw_lock_stats.rw_sx_os_wait_count);

  fprintf(
      file,
      "Spin rounds per wait: %.2f RW-shared,"
      " %.2f RW-excl, %.2f RW-sx\n",
      (double)rw_lock_stats.rw_s_spin_round_count /
          std::max(uint64_t(1), (uint64_t)rw_lock_stats.rw_s_spin_wait_count),
      (double)rw_lock_stats.rw_x_spin_round_count /
          std::max(uint64_t(1), (uint64_t)rw_lock_stats.rw_x_spin_wait_count),
      (double)rw_lock_stats.rw_sx_spin_round_count /
          std::max(uint64_t(1), (uint64_t)rw_lock_stats.rw_sx_spin_wait_count));
}

/**
Prints info of the sync system.
@param file - where to print */
void sync_print(FILE *file) {
#ifdef UNIV_DEBUG
  rw_lock_list_print_info(file);
#endif /* UNIV_DEBUG */

  sync_array_print(file);

  sync_print_wait_info(file);
}

/** Print the filename "basename" e.g., p = "/a/b/c/d/e.cc" -> p = "e.cc"
@param[in]	filename	Name from where to extract the basename
@return the basename */
const char *sync_basename(const char *filename) {
  const char *ptr = filename + strlen(filename) - 1;

  while (ptr > filename && *ptr != '/' && *ptr != '\\') {
    --ptr;
  }

  ++ptr;

  return (ptr);
}

/** String representation of the filename and line number where the
latch was created
@param[in]	id		Latch ID
@param[in]	created		Filename and line number where it was crated
@return the string representation */
std::string sync_mutex_to_string(latch_id_t id, const std::string &created) {
  std::ostringstream msg;

  msg << "Mutex " << sync_latch_get_name(id) << " "
      << "created " << created;

  return (msg.str());
}

/** Enable the mutex monitoring */
void MutexMonitor::enable() {
  /** Note: We don't add any latch meta-data after startup. Therefore
  there is no need to use a mutex here. */

  LatchMetaData::iterator end = latch_meta.end();

  for (LatchMetaData::iterator it = latch_meta.begin(); it != end; ++it) {
    if (*it != NULL) {
      (*it)->get_counter()->enable();
    }
  }
}

/** Disable the mutex monitoring */
void MutexMonitor::disable() {
  /** Note: We don't add any latch meta-data after startup. Therefore
  there is no need to use a mutex here. */

  LatchMetaData::iterator end = latch_meta.end();

  for (LatchMetaData::iterator it = latch_meta.begin(); it != end; ++it) {
    if (*it != NULL) {
      (*it)->get_counter()->disable();
    }
  }
}

/** Reset the mutex monitoring counters */
void MutexMonitor::reset() {
  /** Note: We don't add any latch meta-data after startup. Therefore
  there is no need to use a mutex here. */

  LatchMetaData::iterator end = latch_meta.end();

  for (LatchMetaData::iterator it = latch_meta.begin(); it != end; ++it) {
    if (*it != NULL) {
      (*it)->get_counter()->reset();
    }
  }

  mutex_enter(&rw_lock_list_mutex);

  for (rw_lock_t *rw_lock = UT_LIST_GET_FIRST(rw_lock_list); rw_lock != NULL;
       rw_lock = UT_LIST_GET_NEXT(list, rw_lock)) {
    rw_lock->count_os_wait = 0;
  }

  mutex_exit(&rw_lock_list_mutex);
}
