/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef AGGREGATED_STATS_BUFFER_H
#define AGGREGATED_STATS_BUFFER_H

#include <atomic>
#include "include/my_sqlcommand.h"  // SQLCOM_END

/**
   Similar to System_status_var, implements atomic counters for status variables
   whose values are calculated by aggregating over all available sessions
   (THDs). The single buffer object will store aggregated values for a number of
   THDs belonging to the same shard.
   All counters must use the same atomic data type, to simplify working with
   offsets.

   This mechanism is used to maintain these values in real-time for the purpose
   of reporting telemetry metrics. Existing older mechanism to calculate the
   same data on-demand (for SHOW GLOBAL STATUS) was not modified.
*/
struct aggregated_stats_buffer {
  aggregated_stats_buffer();
  void flush();
  void add_from(aggregated_stats_buffer &shard);
  uint64_t get_counter(std::size_t offset);

  std::atomic_uint64_t com_other;
  std::atomic_uint64_t com_stmt_execute;
  std::atomic_uint64_t com_stmt_close;
  std::atomic_uint64_t com_stmt_fetch;
  std::atomic_uint64_t com_stmt_prepare;
  std::atomic_uint64_t com_stmt_reset;
  std::atomic_uint64_t com_stmt_reprepare;
  std::atomic_uint64_t com_stmt_send_long_data;
  std::atomic_uint64_t com_stat[(unsigned int)SQLCOM_END];

  std::atomic_uint64_t table_open_cache_hits;
  std::atomic_uint64_t table_open_cache_misses;
  std::atomic_uint64_t table_open_cache_overflows;
  std::atomic_uint64_t created_tmp_disk_tables;
  std::atomic_uint64_t created_tmp_tables;
  std::atomic_uint64_t count_hit_tmp_table_size;
  std::atomic_uint64_t max_execution_time_exceeded;
  std::atomic_uint64_t max_execution_time_set;
  std::atomic_uint64_t max_execution_time_set_failed;
  std::atomic_uint64_t opened_tables;
  std::atomic_uint64_t opened_shares;
  std::atomic_uint64_t questions;
  std::atomic_uint64_t secondary_engine_execution_count;
  std::atomic_uint64_t select_full_join_count;
  std::atomic_uint64_t select_full_range_join_count;
  std::atomic_uint64_t select_range_count;
  std::atomic_uint64_t select_range_check_count;
  std::atomic_uint64_t select_scan_count;
  std::atomic_uint64_t long_query_count;
  std::atomic_uint64_t filesort_merge_passes;
  std::atomic_uint64_t filesort_range_count;
  std::atomic_uint64_t filesort_rows;
  std::atomic_uint64_t filesort_scan_count;
  std::atomic_uint64_t bytes_sent;
  std::atomic_uint64_t bytes_received;

  std::atomic_uint64_t ha_commit_count;
  std::atomic_uint64_t ha_delete_count;
  std::atomic_uint64_t ha_discover_count;
  std::atomic_uint64_t ha_external_lock_count;
  std::atomic_uint64_t ha_multi_range_read_init_count;
  std::atomic_uint64_t ha_prepare_count;
  std::atomic_uint64_t ha_read_first_count;
  std::atomic_uint64_t ha_read_key_count;
  std::atomic_uint64_t ha_read_last_count;
  std::atomic_uint64_t ha_read_next_count;
  std::atomic_uint64_t ha_read_prev_count;
  std::atomic_uint64_t ha_read_rnd_count;
  std::atomic_uint64_t ha_read_rnd_next_count;
  std::atomic_uint64_t ha_rollback_count;
  std::atomic_uint64_t ha_savepoint_count;
  std::atomic_uint64_t ha_savepoint_rollback_count;
  std::atomic_uint64_t ha_update_count;
  std::atomic_uint64_t ha_write_count;
};

#endif /* AGGREGATED_STATS_BUFFER_H */
