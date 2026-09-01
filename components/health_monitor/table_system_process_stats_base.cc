/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include <cstring>

#include "table_system_process_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_bits.h"
#include "health_monitor_tables.h"

namespace hm {

int table_system_process_stats_base::rnd_next() {
  m_pos.set_at(&m_next_pos);
  System_Process_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_system_process_stats_base::rnd_pos() {
  System_Process_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

void table_system_process_stats_base::make_row(
    const System_Process_Stats &record) {
  m_row = make_system_process_stats_row(record);
}

int table_system_process_stats_base::read_column_value(PSI_field *field,
                                                       unsigned int index) {
  PSI_uint uint_value;
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.m_timestamp * microseconds_per_second));
      break;
    case 1:  // PID
      big_value = {m_row.m_pid, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 2:  // PROCESS_NAME
      string_srv->set_varchar_utf8mb4(field, m_row.m_process_name.c_str());
      break;
    case 3:  // TID
      big_value = {m_row.m_tid, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 4:  // THREAD_NAME
      string_srv->set_varchar_utf8mb4(field, m_row.m_thread_name.c_str());
      break;
    case 5:  // STATE
      string_srv->set_char_utf8mb4(field, &m_row.m_state, 1);
      break;
    case 6:  // UTIME_MS
      big_value = {m_row.m_utime_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 7:  // STIME_MS
      big_value = {m_row.m_stime_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 8:  // CUTIME_MS
      big_value = {m_row.m_cutime_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 9:  // CSTIME_MS
      big_value = {m_row.m_cstime_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 10:  // NUM_THREADS
      uint_value = {m_row.m_num_threads,
                    system_process_stats_column_is_null(m_row, index)};
      integer_srv->set_unsigned(field, uint_value);
      break;
    case 11:  // VSIZE_BYTES
      big_value = {m_row.m_vsize_bytes,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 12:  // RSS_BYTES
      big_value = {m_row.m_rss_bytes,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 13:  // RSSLIM_BYTES
      big_value = {m_row.m_rsslim_bytes,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 14:  // PROCESSOR
      uint_value = {m_row.m_processor,
                    system_process_stats_column_is_null(m_row, index)};
      integer_srv->set_unsigned(field, uint_value);
      break;
    case 15:  // DELAYACCT_BLKIO_MS
      big_value = {m_row.m_delayacct_blkio_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 16:  // GUEST_TIME_MS
      big_value = {m_row.m_guest_time_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 17:  // CGUEST_TIME_MS
      big_value = {m_row.m_cguest_time_ms,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 18:  // READ_BYTES
      big_value = {m_row.m_read_bytes,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 19:  // WRITE_BYTES
      big_value = {m_row.m_write_bytes,
                   system_process_stats_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

PFS_engine_table_share_proxy *table_system_process_stats_base_init(
    PFS_engine_table_share_proxy &share, const char *table_name,
    Access_control acl) {
  share.m_table_name = table_name;
  share.m_table_name_length = strlen(share.m_table_name);
  share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`PID` bigint unsigned, "
      "`PROCESS_NAME` varchar(255), "
      "`TID` bigint unsigned, "
      "`THREAD_NAME` varchar(255), "
      "`STATE` char(1), "
      "`UTIME_MS` bigint unsigned, "
      "`STIME_MS` bigint unsigned, "
      "`CUTIME_MS` bigint unsigned, "
      "`CSTIME_MS` bigint unsigned, "
      "`NUM_THREADS` int unsigned, "
      "`VSIZE_BYTES` bigint unsigned, "
      "`RSS_BYTES` bigint unsigned, "
      "`RSSLIM_BYTES` bigint unsigned, "
      "`PROCESSOR` int unsigned, "
      "`DELAYACCT_BLKIO_MS` bigint unsigned, "
      "`GUEST_TIME_MS` bigint unsigned, "
      "`CGUEST_TIME_MS` bigint unsigned, "
      "`READ_BYTES` bigint unsigned, "
      "`WRITE_BYTES` bigint unsigned";
  share.m_ref_length = sizeof(table_system_process_stats_base::pos_t);
  share.m_acl = acl;

  return &share;
}

}  // namespace hm
