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

#include "table_system_memory_stats_base.h"

#include "health_monitor.h"
#include "health_monitor_system.h"

namespace hm {

static PFS_engine_table_share_proxy table_share;

// clang-format off

class table_system_memory_stats : public table_system_memory_stats_base {
 public:
  table_system_memory_stats() = default;
  ~table_system_memory_stats() override {
    m_buffer = nullptr; // set in open_table()
  }
  table_system_memory_stats(const table_system_memory_stats &) = default;
  table_system_memory_stats &operator=(const table_system_memory_stats &) = default;
  table_system_memory_stats(table_system_memory_stats &&) = default;
  table_system_memory_stats &operator=(table_system_memory_stats &&) = default;

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();
};

PSI_table_handle *table_system_memory_stats::open_table(PSI_pos **psi_pos) {
  auto *table = new table_system_memory_stats();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  table->m_buffer = Health_Monitor::system_collector()->get_memory_stats_buffer();
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_system_memory_stats::delete_all_rows() { return 0; }

unsigned long long table_system_memory_stats::get_row_count() { return 0; }

PFS_engine_table_share_proxy *table_system_memory_stats_init() {
  PFS_engine_table_proxy &table = table_share.m_proxy_engine_table;

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
  table.open_table = table_system_memory_stats::open_table;
  table.close_table = table_base::close_table;

  table_system_memory_stats_base_init(table_share, "system_memory_stats",
                                      READONLY);
  table_share.delete_all_rows = table_system_memory_stats::delete_all_rows;
  table_share.get_row_count = table_system_memory_stats::get_row_count;

  return &table_share;
}

// clang-format on

}  // namespace hm