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

#include "fx_stats_util.h"

#include <cmath>
#include <utility>  // std::cmp_greater_equal

#if 0
// mean_average() variants for use with advance forecasting features
double mean_average(const std::vector<double> &values, int lo, int hi) {
  if (lo < 0 || std::cmp_greater_equal(hi, values.size()) || lo > hi) {
    return 0.0;
  }
  double sum{0.0};
  for (int i = lo; i <= hi; ++i) {
    sum += values[static_cast<size_t>(i)];
  }
  return ((hi - lo + 1) != 0.0) ? (sum / static_cast<double>(hi - lo + 1))
                                : 0.0;
}

double mean_average(const std::vector<double> &values) {
  double sum{0.0};
  for (double v : values) {
    sum += v;
  }
  return values.empty() ? 0.0 : sum / static_cast<double>(values.size());
}
#endif

double mean_average(const Series &series, int lo, int hi) {
  if (lo < 0 || std::cmp_greater_equal(hi, series.size()) || lo > hi) {
    return 0.0;
  }
  double sum{0.0};
  for (int i = lo; i <= hi; ++i) {
    sum += series[static_cast<size_t>(i)].value;
  }
  return ((hi - lo + 1) != 0) ? sum / static_cast<double>(hi - lo + 1) : 0.0;
}

int64_t postmod(int64_t a, int64_t m) {
  int64_t r{a % m};
  return (r < 0) ? r + m : r;
}

#if 0
// Batch version of Welford_Stats::add() for advanced forecasting features.
double welford_stddev(const std::vector<double> &values) {
  size_t n{0};
  double mean{0.0};
  double M2{0.0};
  for (double x : values) {
    ++n;
    double delta{x - mean};
    mean += delta / n;
    M2 += delta * (x - mean);
  }
  return (n > 1) ? std::sqrt(M2 / (n - 1)) : 0.0;
}
#endif