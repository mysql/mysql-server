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

#include "table_system_cpu_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_system.h"

namespace hm {

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<Stat_Buffer<System_CPU_Stats>> stat_buffer{nullptr};

// clang-format off

class table_system_cpu_stats_history : public table_system_cpu_stats_base {
 public:
  table_system_cpu_stats_history() { m_buffer = stat_buffer; }
  ~table_system_cpu_stats_history() override { m_buffer = nullptr; }

  table_system_cpu_stats_history(const table_system_cpu_stats_history &) = default;
  table_system_cpu_stats_history &operator=(const table_system_cpu_stats_history &) = default;
  table_system_cpu_stats_history(table_system_cpu_stats_history &&) = default;
  table_system_cpu_stats_history &operator=(table_system_cpu_stats_history &&) = default;

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();
};

PSI_table_handle *table_system_cpu_stats_history::open_table(
    PSI_pos **psi_pos) {
  auto *table = new table_system_cpu_stats_history();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_system_cpu_stats_history::delete_all_rows() {
  stat_buffer->reset();
  return 0;
}

unsigned long long int table_system_cpu_stats_history::get_row_count() {
  if (stat_buffer == nullptr) {
    return 0;
  }
  return stat_buffer->max_size();
}

PFS_engine_table_share_proxy *table_system_cpu_stats_history_init(
    std::shared_ptr<Stat_Buffer<System_CPU_Stats>> buffer) {
  PFS_engine_table_proxy &table = table_share.m_proxy_engine_table;
  stat_buffer = std::move(buffer);

  table.rnd_init = table_base::rnd_init;
  table.rnd_next = table_base::rnd_next;
  table.rnd_pos = table_base::rnd_pos;
  table.index_init = nullptr;
  table.index_read = nullptr;
  table.index_next = nullptr;
  table.read_column_value = table_base::read_column_value;
  table.reset_position = table_base::reset_position;
  table.write_column_value = nullptr;
  table.write_row_values = nullptr;
  table.update_column_value = nullptr;
  table.update_row_values = nullptr;
  table.delete_row_values = nullptr;
  table.open_table = table_system_cpu_stats_history::open_table;
  table.close_table = table_base::close_table;

  table_system_cpu_stats_base_init(table_share, "system_cpu_stats_history",
                                   TRUNCATABLE);
  table_share.delete_all_rows = table_system_cpu_stats_history::delete_all_rows;
  table_share.get_row_count = table_system_cpu_stats_history::get_row_count;

  return &table_share;
}

void table_system_cpu_stats_history_deinit() {
  stat_buffer = nullptr;  // release shared pointer
}

// clang-format on

}  // namespace hm