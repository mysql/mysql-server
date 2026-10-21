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

#include "table_disk_forecast_summary.h"

#include <cstring>  // strlen

#include "health_monitor.h"
#include "health_monitor_tables.h"

using namespace hm;

static PFS_engine_table_share_proxy table_share;
static std::shared_ptr<hm::Stat_Buffer<Disk_Forecast_Summary>>
    disk_forecast_summary_buffer{nullptr};

struct table_disk_forecast_summary_row {
  table_disk_forecast_summary_row() = default;
  time_t timestamp{};               // Time of forecast
  double volume_size_mb{};          // Filesystem capacity (MiB)
  double current_usage_mb{};        // Current used space (MiB)
  double current_usage_pct{};       // Used percent
  double trend_per_day_mb{};        // Trend MB/day
  double usage_rate_pct_per_day{};  // % of volume per day
  bool has_depletion_eta{};         // Is ETA meaningful
  double days_to_full{};  // Days to depletion, valid if has_depletion_eta
  int status{};           // Forecast_Status as int
};

class table_disk_forecast_summary : public table_base {
 public:
  typedef table_disk_forecast_summary_row row_t;

  int rnd_next() override;
  int rnd_pos() override;
  int read_column_value(PSI_field *field, unsigned int index) override;
  virtual void make_row(const Disk_Forecast_Summary &record);

  static PSI_table_handle *open_table(PSI_pos **psi_pos);
  static int delete_all_rows();
  static unsigned long long get_row_count();

 protected:
  row_t m_row;
};

PSI_table_handle *table_disk_forecast_summary::open_table(PSI_pos **psi_pos) {
  auto *table = new table_disk_forecast_summary();
  auto *pos = reinterpret_cast<pos_t **>(psi_pos);
  *pos = &table->m_pos;
  return reinterpret_cast<PSI_table_handle *>(table);
}

int table_disk_forecast_summary::rnd_next() {
  m_pos.set_at(&m_next_pos);
  Disk_Forecast_Summary stats;
  if (disk_forecast_summary_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  m_next_pos.set_after(&m_pos);
  return 0;
}

int table_disk_forecast_summary::rnd_pos() {
  Disk_Forecast_Summary stats;
  if (disk_forecast_summary_buffer->read(stats, m_pos.m_index) != 0) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  make_row(stats);
  return 0;
}

int table_disk_forecast_summary::read_column_value(PSI_field *field,
                                                   unsigned int index) {
  PSI_double double_value;
  PSI_enum enum_value;
  PSI_ulonglong big_value;

  switch (index) {
    case 0:  // TIMESTAMP
      timestamp_srv->set2(field, (m_row.timestamp * microseconds_per_second));
      break;
    case 1:  // VOLUME_SIZE_MB
      big_value = {static_cast<uint64_t>(m_row.volume_size_mb), false};
      bigint_srv->set_unsigned(field, big_value);
      break;
    case 2:                                            // CURRENT_USAGE_MB
      double_value = {m_row.current_usage_mb, false};  // NOLINT
      double_srv->set(field, double_value);
      break;
    case 3:                                             // CURRENT_USAGE_PCT
      double_value = {m_row.current_usage_pct, false};  // NOLINT
      double_srv->set(field, double_value);
      break;
    case 4:                                            // TREND_PER_DAY_MB
      double_value = {m_row.trend_per_day_mb, false};  // NOLINT
      double_srv->set(field, double_value);
      break;
    case 5:  // USAGE_RATE_PER_DAY_PCT
      double_value = {m_row.usage_rate_pct_per_day, false};  // NOLINT
      double_srv->set(field, double_value);
      break;
    case 6:  // HAS_ETA
      enum_value = {static_cast<unsigned long long>(m_row.has_depletion_eta),
                    false};
      enum_value.val++;  // bool -> 1-based enum('NO', 'YES')
      enum_srv->set(field, enum_value);
      break;
    case 7:                                        // DAYS_TO_FULL
      double_value = {m_row.days_to_full, false};  // NOLINT
      if (!m_row.has_depletion_eta) {
        double_value.is_null = true;
      }
      double_srv->set(field, double_value);
      break;
    case 8:  // STATUS
      enum_value = {static_cast<unsigned long long>(m_row.status), false};
      enum_srv->set(field, enum_value);
      break;
    default:
      assert(false);
  }
  return 0;
}

void table_disk_forecast_summary::make_row(
    const Disk_Forecast_Summary &record) {
  m_row.timestamp = record.timestamp;
  m_row.volume_size_mb = record.volume_size_mb;
  m_row.current_usage_mb = record.current_usage_mb;
  m_row.current_usage_pct = record.current_usage_pct;
  m_row.trend_per_day_mb = record.trend_per_day_mb;
  m_row.usage_rate_pct_per_day = record.usage_rate_pct_per_day;
  m_row.has_depletion_eta = record.has_depletion_eta;
  m_row.days_to_full = record.days_to_full;
  m_row.status = static_cast<int>(record.status);
}

int table_disk_forecast_summary::delete_all_rows() {
  disk_forecast_summary_buffer->reset();
  return 0;
}

unsigned long long table_disk_forecast_summary::get_row_count() {
  return disk_forecast_summary_buffer->max_size();
}

/**
  Expose the table in the Performance Schema.
*/
PFS_engine_table_share_proxy *table_disk_forecast_summary_init(
    std::shared_ptr<hm::Stat_Buffer<Disk_Forecast_Summary>> buffer) {
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
  table.open_table = table_disk_forecast_summary::open_table;
  table.close_table = table_base::close_table;

  table_share.m_table_name = "disk_forecast_summary";
  table_share.m_table_name_length = strlen(table_share.m_table_name);
  table_share.m_table_definition =
      "`TIMESTAMP` TIMESTAMP NOT NULL, "
      "`VOLUME_SIZE_MB` BIGINT UNSIGNED NOT NULL, "
      "`CURRENT_USAGE_MB` DOUBLE(10,2) NOT NULL, "
      "`CURRENT_USAGE_PCT` DOUBLE(5,2) NOT NULL, "
      "`TREND_PER_DAY_MB` DOUBLE(10,2) NOT NULL, "
      "`USAGE_PER_DAY_PCT` DOUBLE(5,2) NOT NULL, "
      "`HAS_ETA` ENUM ('NO', 'YES') NOT NULL, "
      "`DAYS_TO_FULL` DOUBLE(10,2) NULL, "
      // Note: STATUS is 1-based, Dfx_Status::NONE(0) is omitted
      "`STATUS` ENUM ('NORMAL', 'WARNING', 'CRITICAL') NOT NULL, "
      "PRIMARY KEY (`TIMESTAMP`)";
  table_share.m_ref_length = sizeof(table_disk_forecast_summary::pos_t);
  table_share.m_acl = TRUNCATABLE;
  table_share.delete_all_rows = table_disk_forecast_summary::delete_all_rows;
  table_share.get_row_count = table_disk_forecast_summary::get_row_count;

  disk_forecast_summary_buffer = std::move(buffer);

  return &table_share;
}

void table_disk_forecast_summary_deinit() {
  disk_forecast_summary_buffer = nullptr;  // release shared pointer
}

// clang-format on
