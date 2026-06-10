// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                      // TEST
#include <cstdint>                            // int64_t
#include <cstdlib>                            // abs
#include <random>                             // random_device
#include <vector>                             // vector
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/math/summation.h"             // kahan_sum

namespace {

// ==== Basic tests for mysql::math::kahan_sum ====
//
// This test checks if kahan_sum gives better precision than the usual sum. It
// generates a sequence of `summands` integers, such that their sum does not
// exceed the maximum value of int64_t. Then it computes three values:
//
// - The exact sum, in integer arithmetic.
// - The kahan_sum, using float.
// - The usual sum, using float.
//
// It computes the relative error of kahan_sum and the relative error of the
// usual sum. It repeats this `trial` times. Then it outputs the fraction of
// kahan_sum's average relative error in all the trials, to the usual sum's
// average relative error in all trials. (Which apparently is usually around
// 0.6-0.7% for 100000 summands with this particular distribution.)

constexpr int summands = 100000;
constexpr int trials = 100;

TEST(LibsMathKahanSum, Random) {
  std::random_device rd;  // a seed source for the random number engine
  auto seed = rd();
  MY_SCOPED_TRACE(seed);
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int64_t> dist(
      0, std::numeric_limits<int64_t>::max() / summands);

  double basic_error = 0.0;
  double kahan_error = 0.0;
  for (int trial = 0; trial != trials; ++trial) {
    std::vector<int64_t> values;
    for (int summand = 0; summand != summands; ++summand) {
      values.emplace_back(dist(gen));
    }
    auto exact =
        (double)std::accumulate(values.begin(), values.end(), int64_t{});
    // NOLINTNEXTLINE(bugprone-fold-init-type)
    auto basic = (double)std::accumulate(values.begin(), values.end(), float{});
    double kahan =
        mysql::math::kahan_sum(values.begin(), values.end(), float{});
    basic_error += std::abs(1 - (basic / exact));
    kahan_error += std::abs(1 - (kahan / exact));
  }
  double kahan_improvement = kahan_error / basic_error;
  std::cout << "seed: " << seed << "\n";
  std::cout << "kahan_sum's average relative error, divided by "
               "std::accumulate's relative error: "
            << kahan_improvement << "\n";
}

}  // namespace
