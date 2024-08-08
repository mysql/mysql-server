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

#include "aggregated_stats_buffer.h"
#include "template_utils.h"  // pointer_cast

aggregated_stats_buffer::aggregated_stats_buffer() { flush(); }

void aggregated_stats_buffer::flush() {
  com_other = 0ULL;
  com_stmt_execute = 0ULL;
  com_stmt_close = 0ULL;
  com_stmt_fetch = 0ULL;
  com_stmt_prepare = 0ULL;
  com_stmt_reset = 0ULL;
  com_stmt_reprepare = 0ULL;
  com_stmt_send_long_data = 0ULL;
  for (std::size_t i = 0; i < (std::size_t)SQLCOM_END; i++) com_stat[i] = 0ULL;
  table_open_cache_hits = 0ULL;
  table_open_cache_misses = 0ULL;
  table_open_cache_overflows = 0ULL;
  created_tmp_disk_tables = 0ULL;
  created_tmp_tables = 0ULL;
  max_execution_time_exceeded = 0ULL;
  max_execution_time_set = 0ULL;
  max_execution_time_set_failed = 0ULL;
  opened_tables = 0ULL;
  opened_shares = 0ULL;
  questions = 0ULL;
  secondary_engine_execution_count = 0ULL;
  select_full_join_count = 0ULL;
  select_full_range_join_count = 0ULL;
  select_range_count = 0ULL;
  select_range_check_count = 0ULL;
  select_scan_count = 0ULL;
  long_query_count = 0ULL;
  filesort_merge_passes = 0ULL;
  filesort_range_count = 0ULL;
  filesort_rows = 0ULL;
  filesort_scan_count = 0ULL;
  bytes_sent = 0ULL;
  bytes_received = 0ULL;
  ha_commit_count = 0ULL;
  ha_delete_count = 0ULL;
  ha_discover_count = 0ULL;
  ha_external_lock_count = 0ULL;
  ha_multi_range_read_init_count = 0ULL;
  ha_prepare_count = 0ULL;
  ha_read_first_count = 0ULL;
  ha_read_key_count = 0ULL;
  ha_read_last_count = 0ULL;
  ha_read_next_count = 0ULL;
  ha_read_prev_count = 0ULL;
  ha_read_rnd_count = 0ULL;
  ha_read_rnd_next_count = 0ULL;
  ha_rollback_count = 0ULL;
  ha_savepoint_count = 0ULL;
  ha_savepoint_rollback_count = 0ULL;
  ha_update_count = 0ULL;
  ha_write_count = 0ULL;
}

void aggregated_stats_buffer::add_from(aggregated_stats_buffer &shard) {
  com_other += shard.com_other;
  com_stmt_execute += shard.com_stmt_execute;
  com_stmt_close += shard.com_stmt_close;
  com_stmt_fetch += shard.com_stmt_fetch;
  com_stmt_prepare += shard.com_stmt_prepare;
  com_stmt_reset += shard.com_stmt_reset;
  com_stmt_reprepare += shard.com_stmt_reprepare;
  com_stmt_send_long_data += shard.com_stmt_send_long_data;
  for (std::size_t i = 0; i < (std::size_t)SQLCOM_END; i++)
    com_stat[i] += shard.com_stat[i];
  table_open_cache_hits += shard.table_open_cache_hits;
  table_open_cache_misses += shard.table_open_cache_misses;
  table_open_cache_overflows += shard.table_open_cache_overflows;
  created_tmp_disk_tables += shard.created_tmp_disk_tables;
  created_tmp_tables += shard.created_tmp_tables;
  count_hit_tmp_table_size += shard.count_hit_tmp_table_size;
  max_execution_time_exceeded += shard.max_execution_time_exceeded;
  max_execution_time_set += shard.max_execution_time_set;
  max_execution_time_set_failed += shard.max_execution_time_set_failed;
  opened_tables += shard.opened_tables;
  opened_shares += shard.opened_shares;
  questions += shard.questions;
  secondary_engine_execution_count += shard.secondary_engine_execution_count;
  select_full_join_count += shard.select_full_join_count;
  select_full_range_join_count += shard.select_full_range_join_count;
  select_range_count += shard.select_range_count;
  select_range_check_count += shard.select_range_check_count;
  select_scan_count += shard.select_scan_count;
  long_query_count += shard.long_query_count;
  filesort_merge_passes += shard.filesort_merge_passes;
  filesort_range_count += shard.filesort_range_count;
  filesort_rows += shard.filesort_rows;
  filesort_scan_count += shard.filesort_scan_count;
  bytes_sent += shard.bytes_sent;
  bytes_received += shard.bytes_received;
  ha_commit_count += shard.ha_commit_count;
  ha_delete_count += shard.ha_delete_count;
  ha_discover_count += shard.ha_discover_count;
  ha_external_lock_count += shard.ha_external_lock_count;
  ha_multi_range_read_init_count += shard.ha_multi_range_read_init_count;
  ha_prepare_count += shard.ha_prepare_count;
  ha_read_first_count += shard.ha_read_first_count;
  ha_read_key_count += shard.ha_read_key_count;
  ha_read_last_count += shard.ha_read_last_count;
  ha_read_next_count += shard.ha_read_next_count;
  ha_read_prev_count += shard.ha_read_prev_count;
  ha_read_rnd_count += shard.ha_read_rnd_count;
  ha_read_rnd_next_count += shard.ha_read_rnd_next_count;
  ha_rollback_count += shard.ha_rollback_count;
  ha_savepoint_count += shard.ha_savepoint_count;
  ha_savepoint_rollback_count += shard.ha_savepoint_rollback_count;
  ha_update_count += shard.ha_update_count;
  ha_write_count += shard.ha_write_count;
}

uint64_t aggregated_stats_buffer::get_counter(std::size_t offset) {
  std::atomic_uint64_t *counter =
      pointer_cast<std::atomic_uint64_t *>((char *)this + offset);
  return counter->load();
}
