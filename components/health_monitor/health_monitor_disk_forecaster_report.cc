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

#include "health_monitor_disk_forecaster.h"

#include <algorithm>
#include <cmath>  // std::lround
#include <cstdint>
#include <iomanip>  // setprecision
#include <ranges>
#include <sstream>
#include <string>

#include "health_monitor_log.h"
#include "health_monitor_server.h"
#include "health_monitor_util.h"

// clang-format off
namespace hm {

static inline int clamp_int(int v, int lo, int hi) {
  return std::max(lo, std::min(hi, v));
}

/**
  @brief Approximate a confidence percentage for a point forecast.

  Computes a simple, bounded confidence score in [0, 99] based on the relative
  half–width of the forecast’s prediction interval:

    span  = |CI_UB − CI_LB|
    half  = span / 2
    denom = max(1, |FORECAST_MB|)
    conf  = 100 * (1 − half / denom), then clamped to [0, 99] and rounded

  Rationale:
  - Narrow intervals (small half/denom) yield higher confidence.
  - Very small or zero forecast magnitudes are guarded by a unit floor in the
    denominator to avoid division spikes.
  - The result is intended for logs and status), not for statistical inference,
    so it is deliberately conservative and heuristic.

  Thread-safety:
  - Pure function; does not write shared state.

  @param[in] f  Horizon forecast containing forecast_mb and CI bounds.
  @return int   Confidence percentage in [0, 99], where larger values indicate
                tighter (relatively narrower) intervals around the forecast.
*/
static inline int approximate_confidence(const hm::Disk_Forecast &f) {
  const double span = std::fabs(f.ci_upper_mb - f.ci_lower_mb);
  const double half = span * 0.5;
  const double denom = std::max(1.0, std::fabs(f.forecast_mb));
  const double rel = half / denom;                   // relative half-width
  const double conf = 100.0 * (1.0 - rel);           // 0..100 idealized
  return clamp_int(static_cast<int>(std::round(std::max(0.0, conf))), 0, 99);
}

static inline std::string action_required_for_status(const std::string &status_name) {
  if (status_name == "CRITICAL") {
    return "**CRITICAL**";
  }
  if (status_name == "WARNING") {
    return "Review capacity plan";
  }
  if (status_name == "NORMAL") {
    return "None";
  }
  return "None";
}

/**
  @brief Build a long‑form disk space forecast report as a single formatted string.

  Generates a human‑readable, multi‑section report summarizing current storage
  conditions and projections based on the most recent forecast summary and a set
  of horizon forecasts (e.g., 1h, 24h, 7d). The report is suitable for writing
  to the diagnostics log on the configured cadence (e.g., every 4 hours), and
  may also be emitted immediately when certain events occur (e.g., volume size
  change) to override timer schedules.

  Report content:
  - Header: Report title, generation time, method (Holt‑Winters), server info
  - Headline: Status (NORMAL/WARNING/CRITICAL), current usage %, ETA to
              depletion (if any), daily usage rate (%/day)
  - Projected depletion time and a confidence approximation
  - Forecast timeline: Horizon (minutes/hours/days), projected usage %,
                       status per horizon, and an action hint
  - Usage trend (24h): Visualization and recent rate
  - Immediate actions: Prioritized suggestions
  - Storage details: Capacity, used/free, mount point, filesystem
  - Model performance: Accuracy/MAE, patterns, anomaly status
  - Threshold config: Documented rules and current classification
  - Next forecast update time

  Input expectations:
  - @p summary is the most recent Disk_Forecast_Summary (one row per forecast cycle).
  - @p forecasts is a collection of point forecasts for one or more horizons
    (e.g., 60, 1440, 10080 minutes).

  Assumptions and notes:
  - Confidence is approximated from each forecast’s CI width (relative half‑width),
    then summarized.
  - Some sections (trend visualization, anomaly, detailed accuracy, etc.) are
    placeholders unless supporting metrics are available.
  - Mount point and filesystem are included if known.

  Thread safety:
  - Pure formatter, does not modify forecast or collector states.

  Error handling:
  - If @p forecasts is empty, the report still renders using @p summary and
    marks missing forecast details as N/A.

  @param[in] forecasts  Vector of horizon forecasts (1h/24h/7d). May be empty.
  @param[in] summary    Latest forecast summary (trend, rate, ETA, status).
  @return std::string   Fully formatted report string ready for logging.
*/
std::string Disk_Forecaster::generate_report(std::vector<Disk_Forecast> &forecasts,
                                             Disk_Forecast_Summary &summary) {
  if (forecasts.empty()) {
    return "No forecasts available";
  }

  std::ostringstream out;
  // Header separators
  const auto *const sep = "================================================================================\n";

  // Sort forecasts by horizon for stable output
  std::ranges::sort(forecasts, {}, &Disk_Forecast::horizon_minutes);
  // Compute headline fields
  const std::string server_name = "SERVER-NAME";
  const std::string mysql_version = "MYSQL-VERSION";
  const std::string method_name = "Holt-Winters";
  const std::string gen_time = fmt::format_time(summary.timestamp);
  const std::string status_str = forecast_status_name(summary.status);

  // Days to zero text and projected depletion
  std::string days_to_zero = "N/A";
  std::string projected_depletion = "N/A";
  if (summary.has_depletion_eta) {
    std::ostringstream d; d << std::fixed << std::setprecision(1) << summary.days_to_full;
    days_to_zero = d.str();
    const time_t dep_ts = summary.timestamp + static_cast<time_t>(summary.days_to_full * seconds_per_day);
    projected_depletion = fmt::format_time(dep_ts);
  }

  // Overall “headline” confidence: average of individual confidences (approx)
  int headline_conf = 0;
  if (!forecasts.empty()) {
    int sum_conf = 0;
    for (const auto &f : forecasts) {
      sum_conf += approximate_confidence(f);
    }
    headline_conf = clamp_int(static_cast<int>(std::round(static_cast<double>(sum_conf) / forecasts.size())), 0, 99);
  }

  // Top Header
  out << sep
      << "                      DISK SPACE FORECAST REPORT\n"
      << sep;

  out << "Server: " << server_name
      << "                      Generated: " << gen_time << " UTC\n";
  out << "MySQL Server: " << mysql_version
      << "                         Method: " << method_name << "\n";

  // Headline status
  out << "STATUS: **" << status_str << "**"
      << "    Usage: " << std::fixed << std::setprecision(1) << summary.current_usage_pct << "%"
      << "    Days to Zero: " << (summary.has_depletion_eta ? days_to_zero : "N/A")
      << "    Usage Rate: " << std::setprecision(1) << summary.usage_rate_pct_per_day << "%/day\n";

  out << "PROJECTED DEPLETION: "
      << (summary.has_depletion_eta ? projected_depletion + " (" + std::to_string(headline_conf) + "% confidence)"
                                    : "N/A")
      << "\n";

  // Forecast Timeline
  out << sep
      << "                              FORECAST TIMELINE\n"
      << sep
      << "Time         Usage    Status           Confidence   Action Required\n"
      << "------------------------------------------------------------------------\n";

  for (const auto &f : forecasts) {
    // Usage as percent of volume
    const double usage_pct = (summary.volume_size_mb > 0.0)
                               ? (f.forecast_mb / summary.volume_size_mb) * 100.0
                               : 0.0;
    // Approximate confidence from CI width
    const int conf = approximate_confidence(f);

    // Derive a per-forecast status using same thresholds (approx)
    // Reuse compute_status thresholds via summary-like derivation:
    const double usage_rate = summary.usage_rate_pct_per_day; // same daily rate context
    const bool has_eta = summary.has_depletion_eta;           // ETA computed in summary scope
    const double days_to_full = summary.days_to_full;         // coarse, sufficient for table
    const Dfx_Status s = compute_status(usage_pct, has_eta, days_to_full, usage_rate);
    const std::string s_name = forecast_status_name(s);

    // Action suggestion
    const std::string action = action_required_for_status(s_name);

    // Horizon label verbose
    const std::string hlabel = fmt::format_horizon(f.horizon_minutes, true);

    out << std::left << std::setw(12) << hlabel
        << " " << std::right << std::setw(6) << std::fixed << std::setprecision(1) << usage_pct << "%"
        << "    " << std::left << std::setw(15) << s_name
        << " " << std::right << std::setw(3) << conf << "%"
        << "          " << action << "\n";
  }

  // Usage Trend (placeholder)
  out << sep
      << "                              USAGE TREND (24h)\n"
      << sep
      << "(trend visualization N/A in this build)\n";
  out << "Usage Rate Last 24h: N/A    Last 4h: N/A (N/A)\n";

  // Immediate actions (placeholder)
  out << sep
      << "                            IMMEDIATE ACTIONS\n"
      << sep
      << "PRIORITY 1 (< 4 hours):  Increase available storage\n"
      << "PRIORITY 2 (< 2 hours):  Purge logs/temp files as appropriate\n"
      << "PRIORITY 3 (< 8 hours):  Investigate unusual growth patterns\n";

  // Storage details
  const double free_mb = std::max(0.0, summary.volume_size_mb - summary.current_usage_mb);
  out << sep
      << "                            STORAGE DETAILS\n"
      << sep
      << "Total Capacity:     " << std::setprecision(0) << std::fixed << summary.volume_size_mb << " MiB"
      << "          Mount Point:    " << volume() << "\n"
      << "Used Space:         " << std::setprecision(0) << std::fixed << summary.current_usage_mb << " MiB"
      << "Free Space:          " << std::setprecision(0) << std::fixed << free_mb << " MiB"
      << "Usage Breakdown:\n"
      << "  Database Files:   N/A\n"
      << "  Transaction Logs: N/A\n"
      << "  Temporary Files:  N/A\n"
      << "  System/Other:     N/A\n";

  // Model performance (placeholders for accuracy/stats)
  out << sep
      << "                          MODEL PERFORMANCE\n"
      << sep
      << "Forecast Accuracy:      N/A          Data Points:    " << (7 * 1440) << " (7d)\n"
      << "Mean Absolute Error:    N/A          Last Update:    " << gen_time << " UTC\n"
      << "Pattern Detection:      24h cycle     Weekly cycle:   N/A\n"
      << "Anomaly Status:         N/A           Start time:     N/A\n";

  // Threshold config
  out << sep
      << "                          THRESHOLD CONFIG\n"
      << sep
      << "WARNING:   >=80% usage OR <=7 days to zero\n"
      << "CRITICAL:  >=90% usage OR <=2 days to zero\n"
      << "EMERGENCY: >=95% usage OR <=1 day to zero\n"
      << "Current Status: " << status_str << " (" << std::fixed << std::setprecision(1)
      << summary.current_usage_pct << "% usage, "
      << (summary.has_depletion_eta ? days_to_zero : std::string("N/A")) << " days to zero)\n";

  // Next update time (every 4 hours nominally)
  const time_t next_update = summary.timestamp + (4 * seconds_per_hour);
  out << "Next forecast update: " << fmt::format_time(next_update) << " UTC\n"
      << sep;

  return out.str();
}

/**
  @brief Send disk usage forecast summary to the error log.
  @param forecasts  The most recent forecast record
  @param summary    The last forecast summary
*/
void Disk_Forecaster::log_forecast_status(std::vector<Disk_Forecast> &forecasts [[maybe_unused]],
                                          Disk_Forecast_Summary &summary) {
  if (forecasts.empty()) {
    log_info_message("No forecasts available");
    return;
  }
  std::string msg = format_forecast_summary(summary);
  log_info_message(msg);
}

/**
  @brief Generate a human-readable log message from a Disk_Forecast value.
  @param dfx The forecast to log
  @return  Formatted log message string
*/
std::string Disk_Forecaster::format_disk_forecast(const Disk_Forecast& dfx) {
  std::ostringstream oss;
  oss << "Forecast "
      << std::right << std::setw(7) << fmt::format_horizon(dfx.horizon_minutes, true)
      << ": " << hm::fmt::format_time(dfx.timestamp)
      << "  Target: " << hm::fmt::format_time(dfx.prediction_time)
      << "  Total: " << std::setprecision(0) << std::fixed << dfx.volume_size_mb << " MiB"
      << "  Used: " << std::setprecision(0) << std::fixed << dfx.forecast_mb << " MiB"
      << "  CI: [" << std::setprecision(0) << dfx.ci_lower_mb << ", " << dfx.ci_upper_mb << "]";
  return oss.str();
}

/**
  @brief Generate a human-readable log message from a Disk_Forecast_Summary.
  @param dfxs The summary to log
  @return  Formatted log message string
*/
std::string Disk_Forecaster::format_forecast_summary(const Disk_Forecast_Summary& dfxs) {
  std::ostringstream oss;
  oss << "Forecast Summary"
      << ": " << hm::fmt::format_time(dfxs.timestamp);
  if (dfxs.has_depletion_eta) {
    auto minutes = static_cast<int32_t>(dfxs.days_to_full * minutes_per_day);
    oss << "  ETA: " << fmt::format_horizon(minutes, true);
  } else {
    oss << "  ETA: N/A";
  }
  oss << "  Total: " << std::setprecision(0) << std::fixed << dfxs.volume_size_mb << " MiB"
      << "  Used: " << std::setprecision(0) << dfxs.current_usage_mb << " MiB"
      << " (" << hm::fmt::format_percent(dfxs.current_usage_pct) << ")"
      << "  Trend: " << std::setprecision(1) << dfxs.trend_per_day_mb << " MiB/day"
      << "  Rate: " << std::setprecision(2) << dfxs.usage_rate_pct_per_day << "%/day";
  return oss.str();
}

// clang-format on

}  // namespace hm
