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

#include "health_monitor_disk_forecast_reset.h"

#include <cassert>
#include <limits>

#include "health_monitor_disk_forecaster.h"

namespace hm {

void reset_disk_series(const Disk_writeable_getter &get_writeable,
                       const Disk_writeable_setter &set_writeable,
                       const Disk_series_reset_action &reset_series) {
  const bool writeable = get_writeable();
  set_writeable(false);
  reset_series();
  set_writeable(writeable);
}

unsigned int disk_metric_position_after_reset(const uint64_t total_written) {
  assert(total_written <= std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(total_written);
}

void clear_disk_forecast_retained_state(
    size_t &total_metric_count, time_t &last_forecast_time,
    std::vector<Disk_Forecast> &last_forecasts,
    Disk_Forecast_Summary &last_forecast_summary, uint64_t &total_mb,
    uint64_t &last_total_mb, uint64_t &used_mb, double &used_pct,
    bool &volume_size_changed) {
  total_metric_count = 0;
  last_forecast_time = 0;
  last_forecasts.clear();
  last_forecast_summary = Disk_Forecast_Summary{};
  total_mb = 0;
  last_total_mb = 0;
  used_mb = 0;
  used_pct = 0.0;
  volume_size_changed = false;
}

void clear_disk_forecast_series_no_lock(
    const Disk_Metric_Buffer &metric_buffer,
    const Disk_Metric_Aggregate_Buffer &aggregate_buffer,
    const Disk_Forecast_Buffer &forecast_buffer,
    const Disk_Forecast_Summary_Buffer &summary_buffer,
    Holt_Winters_Forecaster &forecaster, size_t &total_metric_count,
    time_t &last_forecast_time, std::vector<Disk_Forecast> &last_forecasts,
    Disk_Forecast_Summary &last_forecast_summary, uint64_t &total_mb,
    uint64_t &last_total_mb, uint64_t &used_mb, double &used_pct,
    bool &volume_size_changed) {
  metric_buffer->reset_no_lock();
  forecaster.reset_model();
  forecaster.state().next_metric =
      disk_metric_position_after_reset(metric_buffer->total_written());
  if (aggregate_buffer) aggregate_buffer->reset();
  forecast_buffer->reset();
  summary_buffer->reset();

  clear_disk_forecast_retained_state(total_metric_count, last_forecast_time,
                                     last_forecasts, last_forecast_summary,
                                     total_mb, last_total_mb, used_mb, used_pct,
                                     volume_size_changed);
}

void Disk_Forecaster::reset_filesystem_series(
    Disk_forecast_reset_context &context) {
  std::unique_lock<std::mutex> series_lock;
  if (context.series_mutex != nullptr) {
    series_lock = std::unique_lock<std::mutex>(*context.series_mutex);
  }
  reset_disk_series([&context]() { return context.metric_buffer->writeable(); },
                    [&context](bool writeable) {
                      context.metric_buffer->set_writeable(writeable);
                    },
                    [&context]() {
                      rwlock_guard metric_lock(
                          context.metric_buffer->get_rwlock(), Lock_Write,
                          __FILE__, __LINE__);
                      rwlock_guard model_lock(context.model_lock, Lock_Write,
                                              __FILE__, __LINE__);

                      clear_disk_forecast_series_no_lock(
                          context.metric_buffer, context.aggregate_buffer,
                          context.forecast_buffer, context.summary_buffer,
                          *context.forecaster, *context.total_metric_count,
                          *context.last_forecast_time, *context.last_forecasts,
                          *context.last_forecast_summary, *context.total_mb,
                          *context.last_total_mb, *context.used_mb,
                          *context.used_pct, *context.volume_size_changed);
                    });
}

}  // namespace hm
