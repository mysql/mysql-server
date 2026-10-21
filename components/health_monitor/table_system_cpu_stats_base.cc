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

#include "table_system_cpu_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_bits.h"
#include "health_monitor_tables.h"

namespace hm {

int table_system_cpu_stats_base::rnd_next() {
  m_pos.set_at(&m_next_pos);
  System_CPU_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_system_cpu_stats_base::rnd_pos() {
  System_CPU_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

void table_system_cpu_stats_base::make_row(const System_CPU_Stats &record) {
  m_row = {};
  m_row.m_timestamp = record.m_timestamp;
  m_row.m_cpu = record.m_cpu;
  m_row.m_user_ms = record.m_user_ms;
  m_row.m_nice_ms = record.m_nice_ms;
  m_row.m_system_ms = record.m_system_ms;
  m_row.m_idle_ms = record.m_idle_ms;
  m_row.m_iowait_ms = record.m_iowait_ms;
  m_row.m_irq_ms = record.m_irq_ms;
  m_row.m_softirq_ms = record.m_softirq_ms;
  m_row.m_steal_ms = record.m_steal_ms;
  m_row.m_guest_ms = record.m_guest_ms;
  m_row.m_guest_nice_ms = record.m_guest_nice_ms;
  m_row.m_extended_times_available = record.m_extended_times_available;
}

int table_system_cpu_stats_base::read_column_value(PSI_field *field,
                                                   unsigned int index) {
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.m_timestamp * microseconds_per_second));
      break;
    case 1:  // CPU
      string_srv->set_varchar_utf8mb4(field, m_row.m_cpu.c_str());
      break;
    case 2:  // USER_MS
      big_value = {m_row.m_user_ms, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 3:  // NICE_MS
      big_value = {m_row.m_nice_ms, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 4:  // SYSTEM_MS
      big_value = {m_row.m_system_ms, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 5:  // IDLE_MS
      big_value = {m_row.m_idle_ms, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 6:  // IOWAIT_MS
      big_value = {m_row.m_iowait_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 7:  // IRQ_MS
      big_value = {m_row.m_irq_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 8:  // SOFTIRQ_MS
      big_value = {m_row.m_softirq_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 9:  // STEAL_MS
      big_value = {m_row.m_steal_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 10:  // GUEST_MS
      big_value = {m_row.m_guest_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 11:  // GUEST_NICE_MS
      big_value = {m_row.m_guest_nice_ms, !m_row.m_extended_times_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

PFS_engine_table_share_proxy *table_system_cpu_stats_base_init(
    PFS_engine_table_share_proxy &share, const char *table_name,
    Access_control acl) {
  share.m_table_name = table_name;
  share.m_table_name_length = strlen(share.m_table_name);
  share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`CPU` varchar(255), "
      "`USER_MS` bigint unsigned, "
      "`NICE_MS` bigint unsigned, "
      "`SYSTEM_MS` bigint unsigned, "
      "`IDLE_MS` bigint unsigned, "
      "`IOWAIT_MS` bigint unsigned, "
      "`IRQ_MS` bigint unsigned, "
      "`SOFTIRQ_MS` bigint unsigned, "
      "`STEAL_MS` bigint unsigned, "
      "`GUEST_MS` bigint unsigned, "
      "`GUEST_NICE_MS` bigint unsigned";
  share.m_ref_length = sizeof(table_system_cpu_stats_base::pos_t);
  share.m_acl = acl;

  return &share;
}

}  // namespace hm
