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

#include <algorithm>
#include <cmath>  // std::sqrt
#include <cstdint>
#include <functional>
#include <iomanip>  // std::setprecision
#include <numeric>  // std::accumulate
#include <sstream>
#include <vector>

#include "fx_holt_winters_forecaster.h"
#include "fx_sample.h"
#include "health_monitor_disk_forecast_reset.h"
#include "health_monitor_disk_forecaster.h"
#include "health_monitor_log.h"
#include "health_monitor_util.h"

namespace hm {

// clang-format off

// Forecast model R/W lock
mysql_rwlock_t Disk_Forecaster::m_model_lock{};
mysql_rwlock_t * Disk_Forecaster::model_lock() { return &m_model_lock; }

Disk_Forecaster::Status_snapshot Disk_Forecaster::status_snapshot() const {
  rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Read, __FILE__, __LINE__);
  Status_snapshot snapshot;
  snapshot.total_mb = m_total_mb;
  snapshot.used_mb = m_used_mb;
  snapshot.used_pct = m_used_pct;
  snapshot.metric_count = m_total_metric_count;
  snapshot.last_forecast_time = m_last_forecast_time;
  snapshot.forecasts = m_last_forecasts;
  snapshot.summary = m_last_forecast_summary;
  return snapshot;
}

/**
  @brief Determine disk status based on usage and forecast metrics.
  Status levels:
    - Critical if:
      - current usage >= 95%, or
      - depletion ETA <= 1 day, or
      - usage rate >= 15% per day, or
      - current usage >= 90% and depletion ETA <= 2 days
    - Warning if:
      - usage rate >= 8% per day, or
      - current usage >= 80% and depletion ETA <= 7 days
    - Normal otherwise
  @param current_usage_pct   Current disk usage as a percentage of total.
  @param has_depletion_eta   Whether a depletion ETA is available.
  @param days_to_full        Estimated days until full (if slope positive).
  @param usage_rate_pct_per_day Projected daily usage rate as percent of volume.
  @return Dfx_Status The computed status level.
*/
Dfx_Status Disk_Forecaster::compute_status(double current_usage_pct,
                                           bool has_depletion_eta,
                                           double days_to_full,
                                           double usage_rate_pct_per_day) {
  // CRITICAL
  if ((current_usage_pct >= 95.0) ||
      (has_depletion_eta && days_to_full <= 1.0) ||
      (usage_rate_pct_per_day >= 15.0) ||
      (current_usage_pct >= 90.0 && has_depletion_eta && days_to_full <= 2.0)) {
    return Dfx_Status::CRITICAL;
  }
  // WARNING
  if ((usage_rate_pct_per_day >= 8.0) ||
      (current_usage_pct >= 80.0 && has_depletion_eta && days_to_full <= 7.0)) {
    return Dfx_Status::WARNING;
  }
  // NORMAL
  return Dfx_Status::NORMAL;
}

/**
  @brief Compute a summary of the forecast model and current usage.
  Computes:
    - Trend_per_day_mb: projected daily change in used MiB
    - Usage_rate_pct_per_day: projected daily change as percent of volume
    - Days_to_full: estimated days until full (if slope positive)
    - Status: Normal, Warning, or Critical based on thresholds
  @param model             Current Holt-Winters state
  @param volume_size_mb    Total volume size in MiB
  @param current_usage_mb  Most recent disk usage in MiB
  @return Disk_Forecast_Summary  Summary of trend, ETA, and status
  @note The caller must hold the forecast-model read or write lock.
*/
Disk_Forecast_Summary Disk_Forecaster::compute_summary(const Holt_Winters_State &model,
                                                       double volume_size_mb,
                                                       double current_usage_mb) {
  Disk_Forecast_Summary summary{};
  summary.volume_size_mb = volume_size_mb;
  summary.current_usage_mb = current_usage_mb;
  summary.current_usage_pct =
      (volume_size_mb > 0.0)
          ? (summary.current_usage_mb / volume_size_mb) * 100.0
          : 0.0;

  // Trend diagnostics (model.trend is MiB/min; convert to MiB/day)
  summary.trend_per_day_mb = model.trend * static_cast<double>(minutes_per_day);
  summary.usage_rate_pct_per_day =
      (volume_size_mb > 0.0)
          ? (summary.trend_per_day_mb / volume_size_mb) * 100.0
          : 0.0;

  // Depletion ETA policy:
  // - Report ETA when actionable (soon), not based on a fixed minimum slope.
  // - Use maximum reportable ETA window (e.g. 30 days). Can make configurable.
  constexpr double k_max_eta_days = 30.0;  // POLICY: suppress very long ETAs
  const double free_mb = volume_size_mb - summary.current_usage_mb;

  if (free_mb <= 0.0) {
    // Already full or over capacity
    summary.has_depletion_eta = true;
    summary.days_to_full = 0.0;
  } else if (summary.trend_per_day_mb <= 0.0) {
    // Flat or decreasing usage -> no ETA
    summary.has_depletion_eta = false;
    summary.days_to_full = 0.0;
  } else {
    // Raw ETA by simple division
    double eta_days = free_mb / summary.trend_per_day_mb;

    // For now we only enforce a maximum reportable window.
    // TODO: Subtract a conservative daily season margin from free_mb,
    // or use a 'significance check' vs. residual sigma to avoid noise-driven
    // (and worthless) long ETAs.
    if (eta_days <= k_max_eta_days) {
      summary.has_depletion_eta = true;
      summary.days_to_full = std::max(eta_days, 0.0);
    } else {
      // Too far out to be operationally meaningful
      summary.has_depletion_eta = false;
      summary.days_to_full = 0.0;
    }
  }

  // Status evaluation per thresholds
  summary.status = compute_status(summary.current_usage_pct,
                                  summary.has_depletion_eta,
                                  summary.days_to_full,
                                  summary.usage_rate_pct_per_day);
  return summary;
}
/**
  Terminology
    level  - Baseline ('real') value with trend and seasonal components removed.
    season - Fixed periodic cycle, in this case a season = 1 day = 1440 minutes.
    trend  - Slope of the level.
    fitted - A prediction inside the data set, i.e. what we would have predicted
             one step ahead: last level + last trend + seasonal value from one
             day ago at this time. Actual value - fitted value gives us an idea
             of the forecast accuracy.
    residual - Difference between the actual value and the one-step-ahead fitted
             value. We track residuals to estimate σ for prediction intervals.
             Ideally, residuals are like “white noise” and approximately normal.
             If residuals show some kind of structure, then the model should be
             refined (adjust α/β/γ, season length, additive vs. multiplicative,
             account for events, etc.)
    phase  - The index inside the daily (seasonal) cycle: 0..m-1.
             Example: At minute index t, the phase is t % m. Phases let us reuse
             "the same minute tomorrow" information.

  Notation and inputs
    n   - Number of disk space usage values in the time series.
    T   = Last data point.
    y_t - Used disk space at time index t, where t = 0..T (T = n - 1).
    m   - Seasonal (daily) period in samples (1440 for 24h @ 1-min).

    Exponential smoothing parameters, all in [0,1]:
      alpha(α) for level, beta(β) for trend, gamma(γ) for season

    State at time t:
      Level ℓ_t — Baseline (the 'de‑seasonalized' center of the series).
      Trend b_t — Slope per sample, how fast baseline is changing at that point.
      Seasonal s_t — Repeating adjustment based on phase t % m.

    Fitted ŷ_{t|t-1} — The one‑step‑ahead forecast made just before seeing y_t.
    Residual e_t = y_t − ŷ_{t|t-1} — the forecast error at step t.
*/

/**
  @brief Initialize if needed, advance state over this batch, emit 3 forecasts,
         and compute trend/ETA/status summary.

  The 'batch' time series contains only the unprocessed tail of a much larger
  metric stream from the Disk_Metric buffer. This function:
  1. Generates a temporary time series batch from the unprocessed Disk_Metrics.
  2. Initializes the model from if not yet initialized (requires >= 2*m points).
  3. Advances the model over all samples in the batch
  4. Returns forecasts for +1h, +1d, +7d.
  5. Computes trend-per-day, usage-rate %/day, days-to-full, and status.

  @param[out] forecasts Forecasts produced for 1h, 1d, 7d
  @param[out] summary   Output summary with trend/ETA/status
  @return 0 on success, non-zero on error or if not enough data is available
  @note IMPORTANT: Assumes a read lock is held on the disk metric buffer.
*/
int Disk_Forecaster::generate_forecast(std::vector<Disk_Forecast> &forecasts,
                                       Disk_Forecast_Summary &summary) {
  constexpr unsigned int seasonal_period{minutes_per_day};
  // Volume size and current usage are taken from the most recent disk metric
  // and later used to build the forecast summary. Neither is used to generate
  // a disk usage forecast. If we are generating a forecast without new metrics,
  // then we'll use the last recorded values of these for the forecast summary.
  const auto initial_status = status_snapshot();
  double volume_size_mb{static_cast<double>(initial_status.total_mb)};
  double current_usage_mb{static_cast<double>(initial_status.used_mb)};

  if (disk_metric_buffer()->count() < (2 * seasonal_period)) {
    // Not enough data for proper initialization
    return 1;
  }

  Series batch;  // temporary time series of the latest metrics

  // A forecast can be requested before a minimum number of new samples has been
  // collected. (This is different from the minium number of samples required to
  // initalize the forecast model.)
  //
  // With Holt–Winters, forecasting with too few new samples is noisy. For
  // example, a one-step update can cause a disproportionate change in level
  // and an unexpected jump in trend, even if the underlying series is flat.
  //
  // Why 1-point (or small) batches are problematic:
  // - The trend (β) update uses the delta in level across the last step. With a
  //   single sample, any residual (aka noise) looks like a “real” movement and
  //   affects the trend.
  // - The seasonal slot for the current phase (minute of day) will be updated
  //   from just that one observation, so the `fitted = level + trend + season`
  //   term can be off until more phases (minutes) are processed.
  //
  // To guard against single-point noise spikes, we require a minimum number of
  // of new samples before advancing the model.
  const uint64_t min_new_samples{10};

  std::string wait_message;
  const int update_result = [&]() -> int {
    // Scope: r/w model lock

    // Acquire write lock on the forecast model prior to updating it
    rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__,
                      __LINE__);

    //----------------------------------------------------------------------------
    // 1) Build a temporary time series from the unprocessed disk metrics
    //----------------------------------------------------------------------------
    // Acquire a snapshot of the disk metric buffer
    const uint64_t total = disk_metric_buffer()->total_written();  // monotonic
    const size_t count = disk_metric_buffer()->count();  // 0..capacity
    // Logical index of row 0
    const uint64_t oldest = (total >= count) ? (total - count) : 0;
    // Determine logical start: Clamp to oldest if we fell behind and
    // next_metric was overwritten by newer metrics
    auto start = static_cast<uint64_t>(forecaster().state().next_metric);
    // Optional: Log that we missed samples: (oldest - start)
    start = std::max(start, oldest);

    // Check that the minimum number of new samples has been collected
    uint64_t batch_size = total - start;
    if (batch_size < min_new_samples) {
      std::ostringstream message;
      message << "Waiting for " << (min_new_samples - batch_size);
      message << " more samples before generating the next forecast";
      wait_message = message.str();
      return 1;
    }

    // Drain all new logical samples [start, total)
    for (uint64_t logical = start; logical < total; ++logical) {
      int row = -1;
      if (!disk_metric_buffer()->map_logical_to_row_no_lock(logical, row)) {
        // Not available (too old) or not yet written, so just carry on
        continue;
      }
      Disk_Metric metric{};
      if (disk_metric_buffer()->read_no_lock(metric,
                                             static_cast<unsigned>(row)) != 0) {
        return 1;
      }
      batch.emplace_back(metric.timestamp, metric.used_mb);
      // Save the most recent volume size
      if (volume_size_mb != metric.total_mb) {
        volume_size_mb = metric.total_mb;
      }
    }

    // Advance to the latest logical index
    forecaster().state().next_metric = total;  // starting index for next cycle

    //----------------------------------------------------------------------------
    // 2) Initialize the forecast model if needed
    //----------------------------------------------------------------------------
    if (!forecaster().state().initialized && !batch.empty()) {
      forecaster().initialize_model(batch);
      if (!forecaster().state().initialized) {
        forecasts.clear();  // not enough data yet (need >= 2*m)
        summary = Disk_Forecast_Summary{};
        summary.volume_size_mb = volume_size_mb;
        return 1;
      }
    }

    //----------------------------------------------------------------------------
    // 3) Advance over the entire batch, updating the model for each data point
    //----------------------------------------------------------------------------
    if (!batch.empty()) {
      (void)forecaster().advance(batch);
    }

    // Model write lock released, forecast and summary will acquire read locks

    return 0;
  }();  // scope: r/w model lock

  if (!wait_message.empty()) log_info_message(wait_message);
  if (update_result != 0) return 1;

  //----------------------------------------------------------------------------
  // 4) Generate forecasts for 1h, 1d, 1w
  //----------------------------------------------------------------------------
  {
    rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Read, __FILE__,
                      __LINE__);
    const int one_hour{60};
    const int one_day{forecaster().state().m};  // season
    const int one_week{7 * forecaster().state().m};

    Point_Forecast pfh{forecaster().forecast_at(one_hour)};
    Point_Forecast pfd{forecaster().forecast_at(one_day)};
    Point_Forecast pfw{forecaster().forecast_at(one_week)};

    forecasts.clear();
    forecasts.emplace_back(pfh, volume_size_mb);
    forecasts.emplace_back(pfd, volume_size_mb);
    forecasts.emplace_back(pfw, volume_size_mb);

    //------------------------------------------------------------------------
    // 5) Assemble the forecast summary (trend, ETA and status)
    //------------------------------------------------------------------------
    if (!batch.empty()) current_usage_mb = batch.back().value;
    summary =
        compute_summary(forecaster().state(), volume_size_mb, current_usage_mb);
  }
  return 0;
}

/**
  @brief Check if the forecaster is ready to produce forecasts.
  @param log_warning Whether to log a warning if not ready.
  @return True if ready, false otherwise.
 */
bool Disk_Forecaster::ready_to_forecast(bool log_warning) {
  size_t metric_count = 0;
  size_t min_data_points = 0;
  {
    rwlock_guard metric_lock(disk_metric_buffer()->get_rwlock(), Lock_Read,
                             __FILE__, __LINE__);
    rwlock_guard model_lock(Disk_Forecaster::model_lock(), Lock_Read, __FILE__,
                            __LINE__);
    metric_count = disk_metric_buffer()->count();
    min_data_points = (2 * forecaster().state().m);
  }
  if (metric_count < min_data_points) {
    if (log_warning) {
      std::ostringstream msg;
      msg << "Not enough data points for forecasting: Needed: ";
      msg << min_data_points;
      msg << " Current: " << metric_count;
      log_info_message(msg.str());
    }
    return false;
  }
  return true;
}

/**
  Update disk forecast buffers with the lastest forecast and summary.
  @param[in] forecasts  Vector of forecasts for 1h, 1d, 1w
  @param[in] summary    Forecast summary record
*/
void Disk_Forecaster::update_forecast_tables(
    std::vector<Disk_Forecast> &forecasts, Disk_Forecast_Summary &summary) {
  time_t now = time(nullptr);
  for (const auto &forecast : forecasts) {
    Disk_Forecast record = forecast;
    record.timestamp = now;
    disk_forecast_buffer()->write(record);
  }
  summary.timestamp = now;
  disk_forecast_summary_buffer()->write(summary);
}

/**
  Update storage totals, convert to MiB.
  @note The caller must hold the forecast-model write lock.
*/
void Disk_Forecaster::update_storage_totals(const Disk_Metric &disk_metric) {
  if (disk_metric.total_mb == 0.0) {
    return;
  }
  m_total_mb = static_cast<size_t>(disk_metric.total_mb);  // for display only
  m_used_mb = static_cast<size_t>(disk_metric.used_mb);    // for display only
  m_used_pct = utl::percent(m_used_mb, m_total_mb);
  // Generate new forecast if the volume size changed
  if (m_last_total_mb != m_total_mb) {
    m_volume_size_changed = true;
    m_last_total_mb = m_total_mb;
  } else {
    m_volume_size_changed = false;
  }
}

/**
  Reset disk metrics and forecasts before sampling a different filesystem.
*/
void Disk_Forecaster::reset_filesystem_series() {
  Disk_forecast_reset_context context{
      &m_series_mutex,
      disk_metric_buffer(),
      disk_metric_aggregate_buffer(),
      disk_forecast_buffer(),
      disk_forecast_summary_buffer(),
      &forecaster(),
      Disk_Forecaster::model_lock(),
      &m_total_metric_count,
      &m_last_forecast_time,
      &m_last_forecasts,
      &m_last_forecast_summary,
      &m_total_mb,
      &m_last_total_mb,
      &m_used_mb,
      &m_used_pct,
      &m_volume_size_changed};
  reset_filesystem_series(context);
}

// clang-format on
}  // namespace hm
