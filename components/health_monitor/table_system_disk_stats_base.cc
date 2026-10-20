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

#include "table_system_disk_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_bits.h"
#include "health_monitor_tables.h"

namespace hm {

int table_system_disk_stats_base::rnd_next() {
  m_pos.set_at(&m_next_pos);
  System_Disk_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_system_disk_stats_base::rnd_pos() {
  System_Disk_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

void table_system_disk_stats_base::make_row(const System_Disk_Stats &record) {
  m_row = make_system_disk_table_row(record);
}

int table_system_disk_stats_base::read_column_value(PSI_field *field,
                                                    unsigned int index) {
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.m_timestamp * microseconds_per_second));
      break;
    case 1:  // DEVICE
      string_srv->set_varchar_utf8mb4(field, m_row.m_device_name.c_str());
      break;
    case 2:  // READ_COUNT  // Note: "READS" is a reserved word
      big_value = {m_row.m_reads,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 3:  // READ_BYTES
      big_value = {m_row.m_read_bytes,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 4:  // READ_TIME_MS
      big_value = {m_row.m_read_time_ms,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 5:  // WRITE_COUNT
      big_value = {m_row.m_writes,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 6:  // WRITE_BYTES
      big_value = {m_row.m_write_bytes,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 7:  // WRITE_TIME_MS
      big_value = {m_row.m_write_time_ms,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 8:  // FLUSHES
      big_value = {m_row.m_flushes,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 9:  // FLUSH_TIME_MS
      big_value = {m_row.m_flush_time_ms,
                   system_disk_table_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

PFS_engine_table_share_proxy *table_system_disk_stats_base_init(
    PFS_engine_table_share_proxy &share, const char *table_name,
    Access_control acl) {
  share.m_table_name = table_name;
  share.m_table_name_length = strlen(share.m_table_name);
  share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`DEVICE` varchar(255), "
      "`READ_COUNT` bigint unsigned, "
      "`READ_BYTES` bigint unsigned, "
      "`READ_TIME_MS` bigint unsigned, "
      "`WRITE_COUNT` bigint unsigned, "
      "`WRITE_BYTES` bigint unsigned, "
      "`WRITE_TIME_MS` bigint unsigned, "
      "`FLUSHES` bigint unsigned, "
      "`FLUSH_TIME_MS` bigint unsigned";
  share.m_ref_length = sizeof(table_system_disk_stats_base::pos_t);
  share.m_acl = acl;

  return &share;
}

}  // namespace hm
