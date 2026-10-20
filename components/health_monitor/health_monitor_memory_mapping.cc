/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "health_monitor_memory_mapping.h"

#include <cassert>

namespace hm {
namespace {

bool is_available(const mysql::system_info::Result_state state) {
  return state == mysql::system_info::Result_state::available;
}

}  // namespace

bool convert_host_memory_snapshot(
    const mysql::system_info::Host_memory_snapshot &snapshot,
    const time_t timestamp, System_Memory_Stats &output) {
  if (!is_available(snapshot.memory.state())) return false;

  const auto &memory = snapshot.memory.value();
  if (memory.total_bytes == 0 || memory.free_bytes > memory.total_bytes ||
      memory.available_bytes > memory.total_bytes) {
    return false;
  }

  System_Memory_Stats converted;
  converted.m_timestamp = timestamp;
  converted.m_total_bytes = memory.total_bytes;
  converted.m_free_bytes = memory.free_bytes;
  converted.m_avail_bytes = memory.available_bytes;
  converted.m_used_bytes = memory.total_bytes - memory.available_bytes;

  if (is_available(snapshot.breakdown.state())) {
    const auto &breakdown = snapshot.breakdown.value();
    uint64_t remaining = memory.total_bytes;
    if (breakdown.slab_bytes > remaining) return false;
    remaining -= breakdown.slab_bytes;
    if (memory.free_bytes > remaining) return false;
    remaining -= memory.free_bytes;
    if (breakdown.cache_bytes > remaining) return false;
    remaining -= breakdown.cache_bytes;
    if (breakdown.buffer_bytes > remaining) return false;
    converted.m_buffers_bytes = breakdown.buffer_bytes;
    converted.m_cached_bytes = breakdown.cache_bytes;
    converted.m_slab_bytes = breakdown.slab_bytes;
    converted.m_used_bytes = remaining - breakdown.buffer_bytes;
    converted.m_breakdown_available = true;
  }

  if (is_available(snapshot.swap_capacity.state())) {
    const auto &swap_capacity = snapshot.swap_capacity.value();
    if (swap_capacity.free_bytes > swap_capacity.total_bytes) return false;
    converted.m_swap_total_bytes = swap_capacity.total_bytes;
    converted.m_swap_free_bytes = swap_capacity.free_bytes;
    converted.m_swap_used_bytes =
        swap_capacity.total_bytes - swap_capacity.free_bytes;
    converted.m_swap_capacity_available = true;
  }

  if (is_available(snapshot.swap_activity.state())) {
    const auto &swap_activity = snapshot.swap_activity.value();
    converted.m_swap_in = swap_activity.bytes_in;
    converted.m_swap_out = swap_activity.bytes_out;
    converted.m_swap_activity_available = true;
  }

  output = converted;
  return true;
}

bool convert_host_memory_snapshot(
    const mysql::system_info::Host_memory_snapshot &snapshot,
    System_Memory_Info &output) {
  if (!is_available(snapshot.memory.state())) return false;

  const auto &memory = snapshot.memory.value();
  if (memory.total_bytes == 0 || memory.free_bytes > memory.total_bytes ||
      memory.available_bytes > memory.total_bytes) {
    return false;
  }

  System_Memory_Info converted;
  converted.m_total_bytes = memory.total_bytes;
  converted.m_free_bytes = memory.free_bytes;
  converted.m_avail_bytes = memory.available_bytes;

  if (is_available(snapshot.breakdown.state())) {
    converted.m_cached_bytes = snapshot.breakdown.value().cache_bytes;
    converted.m_cache_available = true;
  }

  if (is_available(snapshot.swap_capacity.state())) {
    const auto &swap_capacity = snapshot.swap_capacity.value();
    if (swap_capacity.free_bytes > swap_capacity.total_bytes) return false;
    converted.m_swap_total_bytes = swap_capacity.total_bytes;
    converted.m_swap_free_bytes = swap_capacity.free_bytes;
    converted.m_swap_capacity_available = true;
  }

  output = converted;
  return true;
}

Health_system_memory_row make_health_system_memory_row(
    const System_Memory_Info &record) {
  Health_system_memory_row row;
  row.m_timestamp = record.m_timestamp;
  row.m_total_bytes = record.m_total_bytes;
  row.m_free_bytes = record.m_free_bytes;
  row.m_avail_bytes = record.m_avail_bytes;
  row.m_cached_bytes = record.m_cached_bytes;
  row.m_swap_total_bytes = record.m_swap_total_bytes;
  row.m_swap_free_bytes = record.m_swap_free_bytes;
  row.m_cache_available = record.m_cache_available;
  row.m_swap_capacity_available = record.m_swap_capacity_available;

  double use_pct = record.m_total_bytes - record.m_avail_bytes;
  use_pct /= record.m_total_bytes;
  use_pct *= 100;
  row.m_use_percent = static_cast<float>(use_pct);
  return row;
}

bool health_system_memory_column_is_null(const Health_system_memory_row &row,
                                         const unsigned int index) {
  switch (index) {
    case 5:  // MEMORY_FS_CACHE
      return !row.m_cache_available;
    case 6:  // SWAP_TOTAL
    case 7:  // SWAP_FREE
      return !row.m_swap_capacity_available;
    default:
      assert(false);
      return false;
  }
}

}  // namespace hm
