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

#include "fx_holt_winters_state.h"
#include "fx_point_forecast.h"
#include "fx_sample.h"

/**
  @class Holt_Winters_Forecaster
  @brief Implements the Holt-Winters forecasting algorithm for time series data.

  The Holt-Winters method predicts future values in a time series based on past
  observations. This class provides an implementation of the Holt-Winters
  algorithm, allowing users to:
    - Initialize the model using historical data (@ref initialize_model)
    - Advance the state based on new data (@ref advance)
    - Generate forecasts for future time steps (@ref forecast_at)

  @note This class is designed to work with time series data represented as a
        sequence of @ref Sample objects.
*/
class Holt_Winters_Forecaster {
 public:
  /**
    @brief Initialize Holt-Winters model if sufficient history exists.

    Requires at least two full seasonal cycles. Computes:
    - Initial trend as the average per-step difference between corresponding
      points in the first two seasons.
    - Initial level as the mean of the first season.
    - Initial seasonal components as (y_t - (level + t * trend)).
    @param batch  Time series (must contain at least 2*m samples).
    @note This must be called before any advance() if the model is not
          initialized.
    @note Assumes a write lock on the forecast model is active.
  */
  void initialize_model(const Series &batch);

  /**
    @brief Advance the Holt-Winters model with new observations.

    Process every observation in batch using ingest_one().
    @param batch  Time series of samples, this batch is the unprocessed tail
    @return Advance_Result Number of observations processed.
    @note Assumes a write lock on the forecast model is active.
  */
  Advance_Result advance(const Series &batch);

  /**
    @brief Generate an h-minute-ahead forecast from current state.

    Forecast = level + (k_minutes * trend) + seasonal[(T - m + h) mod m].
    Adds 95% confidence interval using residual standard deviation.

    @param k_minutes  Horizon in minutes (e.g. 60, 1440, 10080).
    @return Point_Forecast Predicted value, CI bounds, and target timestamp.
    @note Seasonal index is aligned one full cycle back: (T - m + h) mod m.
  */
  Point_Forecast forecast_at(int k_minutes);

  /**
    @brief Return the Holt-Winters state for updating.
  */
  Holt_Winters_State &state() { return m_state; }

  /**
    @brief Reset the Holt-Winters model to its default/uninitialized values.

    Clears all fitted parameters and statistics, requiring re-initialization or
    retraining before the forecaster can be used again.
  */
  void reset_model();

 private:
  Holt_Winters_State m_state;
  /**
    @brief Process a single observation into the Holt-Winters state.
    Steps:
    - Read the seasonal component from one full cycle (day) back.
    - Compute one-step-ahead fitted value and residual for this data point (yt).
    - Update level, trend, and seasonal components using smoothing equations.
    - Record residual into Welford stats for confidence intervals.
    @param ts  Timestamp (unix seconds) of this observation
    @param yt  Observed value at time ts
    @param ar  Advance result

    @note The seasonal slot for this phase (minute of day) is overwritten with
          the new seasonal estimate after use. This guarantees 'seasonal
          alignment.' The term 'global index' simply means the index of the
          entire set of disk metrics, of which the current time series batch is
          a subset.
    @note Assumes a write lock on the forecast model is active.
  */
  void ingest_one(int64_t ts, double yt, Advance_Result &ar);
};