/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "table_disk_metric.h"

#include <cstring>  // strlen

#include "health_monitor.h"
#include "health_monitor_tables.h"

using namespace hm;

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<hm::Metric_Buffer<Disk_Metric>> disk_metric_buffer{
    nullptr};

struct table_disk_metric_row {
  table_disk_metric_row() = default;
  time_t timestamp{};
  size_t total_mb{};
  size_t used_mb{};
};

// clang-format off

class table_disk_metric : public table_base {
 public:
  typedef table_disk_metric_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field, unsigned int index) override;
  virtual void make_row(const Disk_Metric &record);

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();

 protected:
  row_t m_row;
};

PSI_table_handle *table_disk_metric::open_table(PSI_pos **psi_pos) {
  auto *table = new table_disk_metric();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_disk_metric::rnd_next() {
  m_pos.set_at(&m_next_pos);
  Disk_Metric stats;
  if (disk_metric_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_disk_metric::rnd_pos() {
  Disk_Metric stats;
  if (disk_metric_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

int table_disk_metric::read_column_value(PSI_field *field,
                                         unsigned int index) {
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.timestamp * microseconds_per_second));
      break;
    case 1:  // VOLUME_SIZE_MB
      big_value = {m_row.total_mb, false}; // NOLINT
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 2:  // USED_SPACE_MB
      big_value = {m_row.used_mb, false}; // NOLINT
      bigint_srv->set_unsigned(field, big_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

void table_disk_metric::make_row(const Disk_Metric &record) {
  m_row.timestamp = record.timestamp;
  m_row.total_mb = record.total_mb;
  m_row.used_mb = record.used_mb;
}

int table_disk_metric::delete_all_rows() {
  disk_metric_buffer->reset();
  return 0;
}

unsigned long long table_disk_metric::get_row_count() {
  return disk_metric_buffer->max_size();
}

/**
  Expose the table in the Performance Schema.
*/
PFS_engine_table_share_proxy *table_disk_metric_init(
    std::shared_ptr<hm::Metric_Buffer<Disk_Metric>> buffer) {
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
  table.open_table = table_disk_metric::open_table;
  table.close_table = table_base::close_table;

  table_share.m_table_name = "disk_metric";
  table_share.m_table_name_length = strlen(table_share.m_table_name);
  table_share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`VOLUME_SIZE_MB` bigint unsigned NOT NULL, "
      "`USED_SPACE_MB` bigint unsigned NOT NULL";
  table_share.m_ref_length = sizeof(table_disk_metric::pos_t);
  table_share.m_acl = TRUNCATABLE;
  table_share.delete_all_rows = table_disk_metric::delete_all_rows;
  table_share.get_row_count = table_disk_metric::get_row_count;

  disk_metric_buffer = std::move(buffer);

  return &table_share;
}

void table_disk_metric_deinit() {
  disk_metric_buffer = nullptr;  // release shared pointer
}

// clang-format on
