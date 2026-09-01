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

#include "table_disk_metric_aggregate.h"

#include <cstring>  // strlen

#include "health_monitor.h"
#include "health_monitor_tables.h"

using namespace hm;

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<hm::Stat_Buffer<Disk_Metric_Aggregate>>
    disk_metric_aggregate_buffer{nullptr};

struct table_disk_metric_aggregate_row {
  table_disk_metric_aggregate_row() = default;
  time_t timestamp{};
  int32_t interval{};
  double usage_pct_min{};
  double usage_pct_max{};
  double usage_pct_avg{};
  double usage_stddev{};
  int32_t sample_count{};
};

// clang-format off

class table_disk_metric_aggregate : public table_base {
 public:
  typedef table_disk_metric_aggregate_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field [[maybe_unused]],
                        unsigned int index [[maybe_unused]]) override;
  virtual void make_row(const Disk_Metric_Aggregate &record [[maybe_unused]]);

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();

 protected:
  row_t m_row;
};

PSI_table_handle *table_disk_metric_aggregate::open_table(PSI_pos **psi_pos [[maybe_unused]]) {
#if 0
  auto *table = new table_disk_metric_aggregate();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
#endif
  return nullptr;
}

int table_disk_metric_aggregate::rnd_next() {
#if 0
  m_pos.set_at(&m_next_pos);
  Disk_Metric_Aggregate stats;
  if (disk_metric_aggregate_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
#endif
  return 0;
}

int table_disk_metric_aggregate::rnd_pos() {
#if 0
  Disk_Metric_Aggregate stats;
  if (disk_metric_aggregate_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
#endif
  return 0;
}

int table_disk_metric_aggregate::read_column_value(PSI_field *field [[maybe_unused]],
                                         unsigned int index [[maybe_unused]]) {
#if 0
  PSI_long int_value;
  PSI_double double_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.timestamp * microseconds_per_second));
      break;
    case 1:  // INTERVAL
      int_value = {m_row.interval, false};
      integer_srv->set(field, int_value);
      break;
    case 2:  // USAGE_PCT_MIN
      double_value = {m_row.usage_pct_min, false};
      double_srv->set(field, double_value);
      break;
    case 3:  // USAGE_PCT_MAX
      double_value = {m_row.usage_pct_max, false};
      double_srv->set(field, double_value);
      break;
    case 4:  // USAGE_PCT_AVG
      double_value = {m_row.usage_pct_avg, false};
      double_srv->set(field, double_value);
      break;
    case 5:  // USAGE_STDDEV
      double_value = {m_row.usage_stddev, false};
      double_srv->set(field, double_value);
      break;
    case 6:  // SAMPLE_COUNT
      int_value = {m_row.sample_count, false};
      integer_srv->set(field, int_value);
      break;
    default:
      assert(false);
  }
  #endif
  return 0;
}

void table_disk_metric_aggregate::make_row(const Disk_Metric_Aggregate &record [[maybe_unused]]) {
#if 0
  m_row.timestamp = record.timestamp;
  m_row.interval = record.interval;
  m_row.usage_pct_min = record.usage_pct_min;
  m_row.usage_pct_max = record.usage_pct_max;
  m_row.usage_pct_avg = record.usage_pct_avg;
  m_row.usage_stddev = record.usage_stddev;
  m_row.sample_count = record.sample_count;
#endif
}

int table_disk_metric_aggregate::delete_all_rows() {
  disk_metric_aggregate_buffer->reset();
  return 0;
}

unsigned long long table_disk_metric_aggregate::get_row_count() {
  return disk_metric_aggregate_buffer->max_size();
}

/**
  Expose the table in the Performance Schema.
*/
PFS_engine_table_share_proxy *table_disk_metric_aggregate_init(
    std::shared_ptr<hm::Stat_Buffer<Disk_Metric_Aggregate>> buffer [[maybe_unused]]) {
#if 0
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
  table.open_table = table_disk_metric_aggregate::open_table;
  table.close_table = table_base::close_table;

  table_share.m_table_name = "disk_metric_aggregate";
  table_share.m_table_name_length = strlen(table_share.m_table_name);
  table_share.m_table_definition =
      "`TIMESTAMP` timestamp NOT NULL, "
      "`INTERVAL` int NOT NULL, "
      "`USAGE_PCT_MIN` DOUBLE(5,2) NOT NULL, "
      "`USAGE_PCT_MAX` DOUBLE(5,2) NOT NULL, "
      "`USAGE_PCT_AVG` DOUBLE(5,2) NOT NULL, "
      "`USAGE_STDDEV` DOUBLE(5,2) NOT NULL, "
      "`SAMPLE_COUNT` int NOT NULL, "
      "PRIMARY KEY (`TIMESTAMP`, `INTERVAL`)";
  table_share.m_ref_length = sizeof(table_disk_metric_aggregate::pos_t);
  table_share.m_acl = TRUNCATABLE;
  table_share.delete_all_rows = table_disk_metric_aggregate::delete_all_rows;
  table_share.get_row_count = table_disk_metric_aggregate::get_row_count;

  disk_metric_aggregate_buffer = std::move(buffer);

  return &table_share;
#endif
  return nullptr;
}

void table_disk_metric_aggregate_deinit() {
  disk_metric_aggregate_buffer = nullptr;  // release shared pointer
}

// clang-format on
