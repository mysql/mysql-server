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

#include <vector>

#include "fx_stats_util.h"

/**
  @struct Holt_Winters_State
  @brief Holt-Winters parameters, fitted state and residual statistics.

  This struct encapsulates the parameters and state variables required by the
  Holt-Winters algorithm to generate forecasts. The members include:
    - Smoothing parameters: @ref m, @ref alpha, @ref beta, @ref gamma
    - Current level and trend: @ref level, @ref trend
    - Seasonal components: @ref season
    - Other state variables: @ref last_idx, @ref last_ts, @ref residual_stats

  @note The state variables are updated as new data is processed, allowing the
        model to adapt to changes in the underlying time series.
*/
struct Holt_Winters_State {
  // Model params
  int m{1440};         // seasonal period (e.g., 1440 for daily data)
  double alpha{0.3};   // smoothing factor for level (0 <= α <= 1)
  double beta{0.1};    // smoothing factor for trend (0 <= β <= 1)
  double gamma{0.05};  // smoothing factor for seasonal component (0 <= γ <= 1)
  double level{0.0};   // current level (baseline value)
  double trend{0.0};   // current trend (slope), MiB/min
  // Seasonal component ring buffer, size m, by phase (minute of day)
  std::vector<double> season;

  // Index of the last processed observation across all batches since model
  // initialization, used to select the seasonal slot when forecasting.
  int64_t last_idx{-1};
  // Timestamp of the last data point
  int64_t last_ts{0};
  // Index of the next metric to process, zero-based
  unsigned int next_metric{0};
  // Time series resolution (cadence) in seconds
  int sample_seconds{60};
  // Stats for the confidence interval, or basically what's left after
  // removing the trend, season and level components, aka 'noise'
  Welford_Stats residual_stats;
  // True if the model has been initialized
  bool initialized{false};
};
