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

#include "table_disk_forecast.h"

#include <cstring>  // strlen

#include "health_monitor.h"
#include "health_monitor_tables.h"

using namespace hm;

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<hm::Stat_Buffer<Disk_Forecast>> disk_forecast_buffer{
    nullptr};

struct table_disk_forecast_row {
  table_disk_forecast_row() = default;
  time_t timestamp{};         // date/time of the forecast
  time_t prediction_time{};   // predicted time of the forecasted values
  int32_t horizon_minutes{};  // forecast horizon in minutes (60, 1440, 10080)
  double volume_size_mb{};    // filesystem capacity (MiB)
  double forecast_mb{};       // predicted used disk space (MiB)
  double ci_lower_mb{};       // lower bound of confidence interval (MiB)
  double ci_upper_mb{};       // upper bound of confidence interval (MiB)
};

// clang-format off

class table_disk_forecast : public table_base {
 public:
  typedef table_disk_forecast_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field, unsigned int index) override;
  virtual void make_row(const Disk_Forecast &record);

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();

 protected:
  row_t m_row;
};

PSI_table_handle *table_disk_forecast::open_table(PSI_pos **psi_pos) {
  auto *table = new table_disk_forecast();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_disk_forecast::rnd_next() {
  m_pos.set_at(&m_next_pos);
  Disk_Forecast stats;
  if (disk_forecast_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_disk_forecast::rnd_pos() {
  Disk_Forecast stats;
  if (disk_forecast_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

int table_disk_forecast::read_column_value(PSI_field *field,
                                         unsigned int index) {
  PSI_double double_value;
  PSI_long int_value;
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.timestamp * microseconds_per_second));
      break;
    case 1:  // PREDICTION_TIME
      timestamp_srv->set2(field, (m_row.prediction_time * microseconds_per_second));
      break;
    case 2:  // HORIZON_MINUTES
      int_value = {m_row.horizon_minutes, false}; // NOLINT
      integer_srv->set(field, int_value);
      break;
    case 3:  // VOLUME_SIZE_MB
      big_value = {static_cast<uint64_t>(m_row.volume_size_mb), false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 4:  // FORECAST_MB
      double_value = {m_row.forecast_mb, false}; // NOLINT
      double_srv->set(field, double_value);
      break;
    case 5:  // CI_LOWER_MB
      double_value = {m_row.ci_lower_mb, false}; // NOLINT
      double_srv->set(field, double_value);
      break;
    case 6:  // CI_UPPER_MB
      double_value = {m_row.ci_upper_mb, false}; // NOLINT
      double_srv->set(field, double_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

void table_disk_forecast::make_row(const Disk_Forecast &record) {
  m_row.timestamp = record.timestamp;
  m_row.prediction_time = record.prediction_time;
  m_row.horizon_minutes = record.horizon_minutes;
  m_row.volume_size_mb = record.volume_size_mb;
  m_row.forecast_mb = record.forecast_mb;
  m_row.ci_lower_mb = record.ci_lower_mb;
  m_row.ci_upper_mb = record.ci_upper_mb;
}

int table_disk_forecast::delete_all_rows() {
  disk_forecast_buffer->reset();
  return 0;
}

unsigned long long table_disk_forecast::get_row_count() {
  return disk_forecast_buffer->max_size();
}

/**
  Expose the table in the Performance Schema.
*/
PFS_engine_table_share_proxy *table_disk_forecast_init(
    std::shared_ptr<hm::Stat_Buffer<Disk_Forecast>> buffer) {
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
  table.open_table = table_disk_forecast::open_table;
  table.close_table = table_base::close_table;

  table_share.m_table_name = "disk_forecast";
  table_share.m_table_name_length = strlen(table_share.m_table_name);
  table_share.m_table_definition =
      "`TIMESTAMP` TIMESTAMP NOT NULL, "
      "`PREDICTION_TIME` TIMESTAMP NOT NULL, "
      "`HORIZON_MINUTES` INT NOT NULL, "
      "`VOLUME_SIZE_MB` BIGINT UNSIGNED NOT NULL, "
      "`FORECAST_MB` DOUBLE(10,2) NOT NULL, "
      "`CI_LOWER_MB` DOUBLE(10,2) NOT NULL, "
      "`CI_UPPER_MB` DOUBLE(10,2) NOT NULL, "
      "PRIMARY KEY (`TIMESTAMP`)";
  table_share.m_ref_length = sizeof(table_disk_forecast::pos_t);
  table_share.m_acl = TRUNCATABLE;
  table_share.delete_all_rows = table_disk_forecast::delete_all_rows;
  table_share.get_row_count = table_disk_forecast::get_row_count;

  disk_forecast_buffer = std::move(buffer);

  return &table_share;
}

void table_disk_forecast_deinit() {
  disk_forecast_buffer = nullptr;  // release shared pointer
}

// clang-format on
