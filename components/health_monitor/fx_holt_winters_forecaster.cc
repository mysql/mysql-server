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

#include "fx_holt_winters_forecaster.h"

#include <cstddef>  // size_t

#include "fx_stats_util.h"

void Holt_Winters_Forecaster::initialize_model(const Series &batch) {
  const int m{m_state.m};
  if (static_cast<int>(batch.size()) < 2 * m) {
    return;
  }

  double sum{0.0};
  for (int i = 0; i < m; ++i) {
    sum += (batch[i + m].value - batch[i].value) / static_cast<double>(m);
  }
  const double b0 = sum / static_cast<double>(m);
  const double avg1 = mean_average(batch, 0, m - 1);
  m_state.level = avg1;
  m_state.trend = b0;

  m_state.season.assign(static_cast<size_t>(m), 0.0);
  for (int i = 0; i < m; ++i) {
    m_state.season[static_cast<size_t>(i)] =
        batch[static_cast<size_t>(i)].value -
        (avg1 + static_cast<double>(i) * b0);
  }

  m_state.last_idx = m - 1;
  m_state.last_ts = batch[static_cast<size_t>(m_state.last_idx)].ts;
  m_state.residual_stats.clear();
  m_state.initialized = true;
}

Advance_Result Holt_Winters_Forecaster::advance(const Series &batch) {
  Advance_Result ar{};
  if (!m_state.initialized) {
    return ar;
  }

  for (size_t i{0}; i < batch.size(); ++i) {
    ingest_one(batch[i].ts, batch[i].value, ar);
  }
  return ar;
}

Point_Forecast Holt_Winters_Forecaster::forecast_at(int k_minutes) {
  Point_Forecast pf{};
  const int m = m_state.m;
  const int64_t T = m_state.last_idx;
  int h = k_minutes % m;

  if (h == 0) {
    h = m;
  };

  const int idx = static_cast<int>(postmod(T - m + h, m));
  const double s_h = m_state.season[static_cast<size_t>(idx)];
  const double yhat =
      m_state.level + (static_cast<double>(k_minutes) * m_state.trend + s_h);
  const double sigma = m_state.residual_stats.sigma();

  pf.horizon_minutes = k_minutes;
  pf.yhat = yhat;
  pf.ci_low = yhat - 1.96 * sigma;
  pf.ci_high = yhat + 1.96 * sigma;
  pf.ts_pred = m_state.last_ts +
               static_cast<int64_t>(k_minutes) * m_state.sample_seconds;

  return pf;
}

void Holt_Winters_Forecaster::reset_model() {
  m_state = Holt_Winters_State();  // value-initialize to defaults
  m_state.season.clear();          // ensure no old entries remain
  m_state.residual_stats.clear();  // clear stddev stats
}

void Holt_Winters_Forecaster::ingest_one(int64_t ts, double yt,
                                         Advance_Result &ar) {
  const int phase = static_cast<int>(postmod(m_state.last_idx + 1, m_state.m));
  const double s_tm = m_state.season[static_cast<size_t>(phase)];
  const double fitted = m_state.level + m_state.trend + s_tm;
  const double residual = yt - fitted;
  // Update running statistics (count/mean/M2) with the current residual
  m_state.residual_stats.add(residual);

  const double new_level{
      (m_state.alpha * (yt - s_tm)) +
      ((1.0 - m_state.alpha) * (m_state.level + m_state.trend))};
  const double new_trend{(m_state.beta * (new_level - m_state.level)) +
                         ((1.0 - m_state.beta) * m_state.trend)};
  const double new_season{(m_state.gamma * (yt - new_level)) +
                          ((1.0 - m_state.gamma) * s_tm)};

  m_state.season[static_cast<size_t>(phase)] = new_season;
  m_state.level = new_level;
  m_state.trend = new_trend;

  m_state.last_idx += 1;
  m_state.last_ts = ts;
  ar.processed += 1;
}
