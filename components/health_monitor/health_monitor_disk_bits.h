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

#pragma once

#include "fx_point_forecast.h"
#include "health_monitor_constants.h"
#include "health_monitor_metric_buffer.h"  // and Stat_Buffer

namespace hm {

/**
  Disk Info
  Record structure for health_block_device.
*/
struct Disk_Info {
  Disk_Info() = default;
  std::string device_name;
  time_t timestamp{};
  uint64_t total_bytes{};
  uint64_t avail_bytes{};
  std::string mount_point;
  std::string mount_source;
  std::string filesystem_id;
  std::string filesystem_type;
  // True if block device, e.g. /dev/sdc vs /dev/shm
  bool block_device{false};
  bool partition{false};
};

using Disk_Stats_Buffer = std::shared_ptr<Stat_Buffer<Disk_Info>>;

/**
  Disk metric time series.
*/
struct Disk_Metric {
  Disk_Metric() = default;
  Disk_Metric(time_t ts, uint64_t total_bytes, uint64_t used_bytes)
      : timestamp(ts),
        total_mb(static_cast<double>(total_bytes / MiB)),
        used_mb(static_cast<double>(used_bytes / MiB)) {}
  Disk_Metric(const Disk_Info &disk_info)
      : timestamp(disk_info.timestamp),
        total_mb(static_cast<double>(disk_info.total_bytes / MiB)),
        used_mb(static_cast<double>(
            (disk_info.total_bytes - disk_info.avail_bytes) / MiB)) {}
  time_t timestamp{};
  double total_mb{};
  double used_mb{};
};

using Disk_Metric_Buffer = std::shared_ptr<Metric_Buffer<Disk_Metric>>;

/**
  Aggregated disk metric.
*/
struct Disk_Metric_Aggregate {
  time_t timestamp{};      // start time of aggregation window
  int32_t interval{};      // aggregation window size (minutes)
  double usage_pct_min{};  // minimum disk usage percent
  double usage_pct_max{};  // maximum disk usage percent
  double usage_pct_avg{};  // average disk usage percent
  double usage_stddev{};   // standard deviation of usage percent
  int32_t sample_count{};  // samples in aggregation window
};

using Disk_Metric_Aggregate_Buffer =
    std::shared_ptr<Stat_Buffer<Disk_Metric_Aggregate>>;

// Forecast depletion ETA classifications
enum class Dfx_Status { NONE, NORMAL, WARNING, CRITICAL };

/**
  Forecast record. Values are defined as double to preserve accuracy over
  multiple iterations.
*/
struct Disk_Forecast {
  Disk_Forecast() = default;
  Disk_Forecast(const Point_Forecast &pf, double vol_size_mb)
      : timestamp(time(nullptr)),
        prediction_time(static_cast<time_t>(pf.ts_pred)),
        horizon_minutes(pf.horizon_minutes),
        volume_size_mb(vol_size_mb),
        forecast_mb(pf.yhat),
        ci_lower_mb(pf.ci_low),
        ci_upper_mb(pf.ci_high) {}
  time_t timestamp{};         // time of forecast
  time_t prediction_time{};   // predicted time of the forecasted values
  int32_t horizon_minutes{};  // forecast horizon in minutes (60, 1440, 10080)
  double volume_size_mb{};    // filesystem capacity (MiB)
  double forecast_mb{};       // predicted available disk space (MiB)
  double ci_lower_mb{};       // lower bound of confidence interval (MiB)
  double ci_upper_mb{};       // upper bound of confidence interval (MiB)
};

using Disk_Forecast_Buffer = std::shared_ptr<Stat_Buffer<Disk_Forecast>>;

struct Disk_Forecast_Summary {
  time_t timestamp{};  // time of forecast
  // Inputs echoed
  double volume_size_mb{0.0};
  double current_usage_mb{0.0};
  double current_usage_pct{0.0};
  // Trend diagnostics
  double trend_per_day_mb{0.0};        // signed MB/day
  double usage_rate_pct_per_day{0.0};  // signed % of volume per day
  // Depletion ETA
  bool has_depletion_eta{false};
  double days_to_full{0.0};  // valid iff has_depletion_eta
  // Status
  Dfx_Status status{Dfx_Status::NONE};
};

using Disk_Forecast_Summary_Buffer =
    std::shared_ptr<Stat_Buffer<Disk_Forecast_Summary>>;

}  // namespace hm
