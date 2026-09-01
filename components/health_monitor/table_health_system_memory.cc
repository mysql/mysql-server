/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "table_base.h"

#include "health_monitor.h"
#include "health_monitor_memory.h"
#include "health_monitor_memory_mapping.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_tables.h"

using namespace hm;

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<Stat_Buffer<System_Memory_Info>> stat_buffer{nullptr};

// clang-format off

class table_health_system_memory : public table_base {
 public:
  typedef Health_system_memory_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field, unsigned int index) override;
  void make_row(const System_Memory_Info &record);

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();

 protected:
  row_t m_row;
};

PSI_table_handle *table_health_system_memory::open_table(PSI_pos **psi_pos) {
  auto *table = new table_health_system_memory();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_health_system_memory::rnd_next() {
  m_pos.set_at(&m_next_pos);
  System_Memory_Info stats;
  if (stat_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_health_system_memory::rnd_pos() {
  System_Memory_Info stats;
  if (stat_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

int table_health_system_memory::read_column_value(PSI_field *field,
                                                  unsigned int index) {
  PSI_ulonglong big_value;
  PSI_decimal decimal_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.m_timestamp * microseconds_per_second));
      break;
    case 1:  // TOTAL_MEMORY
      big_value = {m_row.m_total_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 2:  // AVAILABLE
      big_value = {m_row.m_avail_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 3:  // USE_PERCENT
      decimal_value = {m_row.m_use_percent, false};
      decimal_srv->set(field, decimal_value);
      break;
    case 4:  // MEMORY_FREE
      big_value = {m_row.m_free_bytes, false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 5:  // MEMORY_FS_CACHE
      big_value = {m_row.m_cached_bytes,
                   health_system_memory_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 6:  // SWAP_TOTAL
      big_value = {m_row.m_swap_total_bytes,
                   health_system_memory_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 7:  // SWAP_FREE
      big_value = {m_row.m_swap_free_bytes,
                   health_system_memory_column_is_null(m_row, index)};
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

void table_health_system_memory::make_row(const System_Memory_Info &record) {
  m_row = make_health_system_memory_row(record);
}

int table_health_system_memory::delete_all_rows() {
  stat_buffer->reset();
  return 0;
}

unsigned long long table_health_system_memory::get_row_count() {
  return stat_buffer->max_size();
}

/* Expose the table in the performance schema */

PFS_engine_table_share_proxy *table_health_system_memory_init(
    std::shared_ptr<Stat_Buffer<System_Memory_Info>> buffer) {
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
  table.open_table = table_health_system_memory::open_table;
  table.close_table = table_base::close_table;

  table_share.m_table_name = "health_system_memory";
  table_share.m_table_name_length = strlen(table_share.m_table_name);
  table_share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`TOTAL_MEMORY` bigint unsigned NOT NULL, "
      "`AVAILABLE` bigint unsigned NOT NULL, "
      "`USE_PERCENT` decimal(5,2) NOT NULL, "
      "`MEMORY_FREE` bigint unsigned NOT NULL, "
      "`MEMORY_FS_CACHE` bigint unsigned, "
      "`SWAP_TOTAL` bigint unsigned, "
      "`SWAP_FREE` bigint unsigned";
  table_share.m_ref_length = sizeof(table_health_system_memory::pos_t);
  table_share.m_acl = TRUNCATABLE;
  table_share.delete_all_rows = table_health_system_memory::delete_all_rows;
  table_share.get_row_count = table_health_system_memory::get_row_count;

  stat_buffer = std::move(buffer);

  return &table_share;
}

void table_health_system_memory_deinit() {
  stat_buffer = nullptr;  // release shared pointer
}

// clang-format on
