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

#ifndef TABLE_SYSTEM_MEMORY_STATS_BASE_INCLUDED
#define TABLE_SYSTEM_MEMORY_STATS_BASE_INCLUDED

#include <mysql/components/services/pfs_plugin_table_service.h>

#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_bits.h"
#include "table_base.h"

namespace hm {

struct table_system_memory_stats_row {
  table_system_memory_stats_row() = default;
  time_t m_timestamp{0};
  uint64_t m_total_bytes{0};
  uint64_t m_free_bytes{0};
  uint64_t m_avail_bytes{0};
  uint64_t m_used_bytes{0};
  uint64_t m_buffers_bytes{0};
  uint64_t m_cached_bytes{0};
  uint64_t m_slab_bytes{0};
  uint64_t m_swap_total_bytes{0};
  uint64_t m_swap_free_bytes{0};
  uint64_t m_swap_used_bytes{0};
  uint64_t m_swap_in{0};
  uint64_t m_swap_out{0};
  bool m_breakdown_available{false};
  bool m_swap_capacity_available{false};
  bool m_swap_activity_available{false};
};

class table_system_memory_stats_base : public table_base {
 public:
  typedef table_system_memory_stats_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field, unsigned int index) override;
  virtual void make_row(const System_Memory_Stats &record);

 protected:
  row_t m_row;
  std::shared_ptr<Stat_Buffer<System_Memory_Stats>> m_buffer;
};

PFS_engine_table_share_proxy *table_system_memory_stats_base_init(
    PFS_engine_table_share_proxy &share, const char *table_name,
    Access_control acl);

}  // namespace hm

#endif  // TABLE_SYSTEM_MEMORY_STATS_BASE_INCLUDED
