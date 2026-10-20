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

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "fx_holt_winters_forecaster.h"
#include "health_monitor_disk_bits.h"

namespace hm {

/**
  The context reset atomically discards all forecasting state, and is used when
  the monitored filesystem changes.

  Metrics and model state from different filesystems must not share the same
  time series. If the filesystem changes, then we clear the metric, model,
  forecast, summary and reporting state before accepting samples for the
  new filesystem.

  Preserve the metric count and advance the model so old logical indexes are
  not reused.

  A capacity change (resize) for the same filesystem does not reset the series.
*/

struct Disk_forecast_reset_context {
  std::mutex *series_mutex;
  Disk_Metric_Buffer metric_buffer;
  Disk_Metric_Aggregate_Buffer aggregate_buffer;
  Disk_Forecast_Buffer forecast_buffer;
  Disk_Forecast_Summary_Buffer summary_buffer;
  Holt_Winters_Forecaster *forecaster;
  mysql_rwlock_t *model_lock;
  size_t *total_metric_count;
  time_t *last_forecast_time;
  std::vector<Disk_Forecast> *last_forecasts;
  Disk_Forecast_Summary *last_forecast_summary;
  uint64_t *total_mb;
  uint64_t *last_total_mb;
  uint64_t *used_mb;
  double *used_pct;
  bool *volume_size_changed;
};

using Disk_writeable_getter = std::function<bool()>;
using Disk_writeable_setter = std::function<void(bool)>;
using Disk_series_reset_action = std::function<void()>;

/**
  Pause disk metric writes, reset the series and restore the prior write state.

  @param[in] get_writeable Write state reader
  @param[in] set_writeable Write state setter
  @param[in] reset_series Locked series reset action
*/
void reset_disk_series(const Disk_writeable_getter &get_writeable,
                       const Disk_writeable_setter &set_writeable,
                       const Disk_series_reset_action &reset_series);

/**
  Return the next logical metric position after clearing retained rows.

  @param[in] total_written Monotonic count of metrics written
  @return Logical position for the first metric after reset
*/
unsigned int disk_metric_position_after_reset(uint64_t total_written);

void clear_disk_forecast_retained_state(
    size_t &total_metric_count, time_t &last_forecast_time,
    std::vector<Disk_Forecast> &last_forecasts,
    Disk_Forecast_Summary &last_forecast_summary, uint64_t &total_mb,
    uint64_t &last_total_mb, uint64_t &used_mb, double &used_pct,
    bool &volume_size_changed);

/**
  Clear metric, model, forecast and reporting state.

  The caller must hold the buffer write lock and forecast model write lock.
*/
void clear_disk_forecast_series_no_lock(
    const Disk_Metric_Buffer &metric_buffer,
    const Disk_Metric_Aggregate_Buffer &aggregate_buffer,
    const Disk_Forecast_Buffer &forecast_buffer,
    const Disk_Forecast_Summary_Buffer &summary_buffer,
    Holt_Winters_Forecaster &forecaster, size_t &total_metric_count,
    time_t &last_forecast_time, std::vector<Disk_Forecast> &last_forecasts,
    Disk_Forecast_Summary &last_forecast_summary, uint64_t &total_mb,
    uint64_t &last_total_mb, uint64_t &used_mb, double &used_pct,
    bool &volume_size_changed);

}  // namespace hm
