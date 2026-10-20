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

#include "table_system_memory_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_bits.h"
#include "health_monitor_tables.h"

namespace hm {

int table_system_memory_stats_base::rnd_next() {
  m_pos.set_at(&m_next_pos);
  System_Memory_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_system_memory_stats_base::rnd_pos() {
  System_Memory_Stats stats;
  if (m_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

void table_system_memory_stats_base::make_row(
    const System_Memory_Stats &record) {
  m_row = {};
  m_row.m_timestamp = record.m_timestamp;
  m_row.m_total_bytes = record.m_total_bytes;
  m_row.m_free_bytes = record.m_free_bytes;
  m_row.m_used_bytes = record.m_used_bytes;
  m_row.m_avail_bytes = record.m_avail_bytes;
  m_row.m_buffers_bytes = record.m_buffers_bytes;
  m_row.m_cached_bytes = record.m_cached_bytes;
  m_row.m_slab_bytes = record.m_slab_bytes;
  m_row.m_swap_total_bytes = record.m_swap_total_bytes;
  m_row.m_swap_free_bytes = record.m_swap_free_bytes;
  m_row.m_swap_used_bytes = record.m_swap_used_bytes;
  m_row.m_swap_in = record.m_swap_in;
  m_row.m_swap_out = record.m_swap_out;
  m_row.m_breakdown_available = record.m_breakdown_available;
  m_row.m_swap_capacity_available = record.m_swap_capacity_available;
  m_row.m_swap_activity_available = record.m_swap_activity_available;
}

int table_system_memory_stats_base::read_column_value(PSI_field *field,
                                                      unsigned int index) {
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.m_timestamp * microseconds_per_second));
      break;
    case 1:  // TOTAL_BYTES
      big_value = {m_row.m_total_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 2:  // FREE_BYTES
      big_value = {m_row.m_free_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 3:  // USED_BYTES
      big_value = {m_row.m_used_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 4:  // AVAILABLE_BYTES
      big_value = {m_row.m_avail_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 5:  // BUFFERS_BYTES
      big_value = {m_row.m_buffers_bytes, !m_row.m_breakdown_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 6:  // CACHED_BYTES
      big_value = {m_row.m_cached_bytes, !m_row.m_breakdown_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 7:  // SLAB_BYTES
      big_value = {m_row.m_slab_bytes, !m_row.m_breakdown_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 8:  // SWAP_TOTAL_BYTES
      big_value = {m_row.m_swap_total_bytes, !m_row.m_swap_capacity_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 9:  // SWAP_FREE_BYTES
      big_value = {m_row.m_swap_free_bytes, !m_row.m_swap_capacity_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 10:  // SWAP_USED_BYTES
      big_value = {m_row.m_swap_used_bytes, !m_row.m_swap_capacity_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 11:  // SWAP_IN
      big_value = {m_row.m_swap_in, !m_row.m_swap_activity_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 12:  // SWAP_OUT
      big_value = {m_row.m_swap_out, !m_row.m_swap_activity_available};
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

PFS_engine_table_share_proxy *table_system_memory_stats_base_init(
    PFS_engine_table_share_proxy &share, const char *table_name,
    Access_control acl) {
  share.m_table_name = table_name;
  share.m_table_name_length = strlen(share.m_table_name);
  share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`TOTAL_BYTES` bigint unsigned NOT NULL, "
      "`FREE_BYTES` bigint unsigned NOT NULL, "
      "`USED_BYTES` bigint unsigned NOT NULL, "
      "`AVAILABLE_BYTES` bigint unsigned NOT NULL, "
      "`BUFFERS_BYTES` bigint unsigned, "
      "`CACHED_BYTES` bigint unsigned, "
      "`SLAB_BYTES` bigint unsigned, "
      "`SWAP_TOTAL_BYTES` bigint unsigned, "
      "`SWAP_FREE_BYTES` bigint unsigned, "
      "`SWAP_USED_BYTES` bigint unsigned, "
      "`SWAP_IN` bigint unsigned, "
      "`SWAP_OUT` bigint unsigned";
  share.m_ref_length = sizeof(table_system_memory_stats_base::pos_t);
  share.m_acl = acl;

  return &share;
}

}  // namespace hm
