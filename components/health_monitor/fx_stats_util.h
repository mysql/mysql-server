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

#include <cmath>    // std::sqrt
#include <cstdint>  // uint64_t
#include <vector>

#include "fx_sample.h"  // Sample, Series

/**
  @brief Compute a modulo with a non-negative result ("positive modulo").

  In C++, the % operator can yield negative remainders if the dividend is
  negative, which is unsuitable for indexing arrays. This helper ensures the
  result is always in the range [0, m-1].

  @param a The integer value (can be negative).
  @param m The modulus (must be positive).
  @return int64_t The remainder of a divided by m, guaranteed non-negative.
  @note Commonly used to wrap seasonal indices in Holt-Winters, ensuring
        valid access into the seasonal component array.
  @code
  postmod(5, 4);   // returns 1
  postmod(-1, 4);  // returns 3
  postmod(-5, 4);  // returns 3
  @endcode
*/
int64_t postmod(int64_t a, int64_t m);

#if 0
// Variants for use with advanced forecasting features
/**
  @brief Compute the arithmetic mean of a subsequence of samples.
  @param values  Vector of double values
  @param lo  Inclusive start index within the series.
  @param hi  Inclusive end index within the series.
  @return double The average value of y[lo..hi].

  @warning No bounds checking: caller must ensure 0 <= lo <= hi < series.size().
  @note Used for initializing Holt-Winters state (level and detrended season).
*/
double mean_average(const std::vector<double> &values, int lo, int hi);

/**
  @brief Compute the arithmetic mean of a subsequence of samples.
  @param values  Vector of double values
  @return double The average value of y[lo..hi].

  @warning No bounds checking: caller must ensure 0 <= lo <= hi < series.size().
  @note Used for initializing Holt-Winters state (level and detrended season).
*/
double mean_average(const std::vector<double> &values);
#endif

/**
  @brief Compute the arithmetic mean of a subsequence of samples.
  @param series The full time series (vector of samples).
  @param lo  Inclusive start index within the series.
  @param hi  Inclusive end index within the series.
  @return The mean for the inclusive range [lo, hi], or 0.0 if the range is
  invalid.
  @note Used for initializing Holt-Winters state (level and detrended season).
*/
double mean_average(const Series &series, int lo, int hi);

/**
  @struct Welford_Stats
  @brief Maintains running statistics for a stream of values using Welford's
  online algorithm.

  This struct provides a way to calculate the mean and variance of a dataset
  without storing the entire dataset in memory. It is suitable for large or
  streaming datasets.
  The statistics are updated incrementally as new values are added using the
  @ref add method.

*/
struct Welford_Stats {
  uint64_t count{0};  ///< Number of values processed so far
  double mean{0.0};   ///< Current mean of the values
  double m2{0.0};     ///< Sum of squared deviations from the mean

  /**
    @brief Adds a new value to the statistics.
    @param x The new value to add.
    @note add() updates running statistics online (count/mean/M2) one residual
          at a time, and sigma() returns the stddev when needed. This avoids
          having to store all residuals and then compute the stddev.
  */
  void add(double x) {
    ++count;
    double d = x - mean;
    mean += d / count;
    m2 += d * (x - mean);
  }

  /**
    @brief Checks if the statistics are ready (i.e., more than one value has
           been processed).
    @return True if the statistics are ready, false otherwise.
  */
  [[nodiscard]] bool ready() const { return count > 1; }

  /**
    @brief Returns the sample standard deviation of the values.
    @return The sample standard deviation.
  */
  [[nodiscard]] double sigma() const {
    return ready() ? std::sqrt(m2 / (count - 1)) : 0.0;
  }

  /**
    @brief Resets the statistics to their initial state.
  */
  void clear() {
    count = 0;
    mean = 0.0;
    m2 = 0.0;
  }
};

#if 0
/**
  @brief Compute the sample standard deviation using Welford's online algorithm.

  Welford's algorithm is a method for calculating the variance and standard
  deviation of a dataset. It avoids the need to store the entire dataset
  in memory, and is suitable for large or streaming datasets.

  @param values A vector of double values representing the dataset.
  @return The sample standard deviation of the input values.

  @note Returns 0.0 if the input vector contains fewer than two elements.
        This is the batch/one-shot counterpart to Welford_Stats::add(), and will
        be used in advanced forecasting features, e.g. aggregation, parameter
        tuning, etc.
 */
double welford_stddev(const std::vector<double> &values);
#endif
