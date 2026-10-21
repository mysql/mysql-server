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

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "../health_monitor_util.h"

namespace hm {
namespace {

TEST(HealthMonitorUtil, MeasuresIncreasesAndDecreases) {
  EXPECT_DOUBLE_EQ(1.0, utl::change_percent(100, 101));
  EXPECT_DOUBLE_EQ(1.0, utl::change_percent(100, 99));
  EXPECT_DOUBLE_EQ(0.0, utl::change_percent(100, 100));
  EXPECT_DOUBLE_EQ(100.0, utl::change_percent(100, 0));
  EXPECT_DOUBLE_EQ(200.0, utl::change_percent(100, 300));
}

TEST(HealthMonitorUtil, ComparesChangesWithStatusThreshold) {
  constexpr double threshold = 5.0;
  EXPECT_LT(utl::change_percent(100, 104), threshold);
  EXPECT_GE(utl::change_percent(100, 105), threshold);
  EXPECT_GT(utl::change_percent(100, 106), threshold);
  EXPECT_LT(utl::change_percent(100, 96), threshold);
  EXPECT_GE(utl::change_percent(100, 95), threshold);
  EXPECT_GT(utl::change_percent(100, 94), threshold);
}

TEST(HealthMonitorUtil, HandlesMissingBaseline) {
  EXPECT_DOUBLE_EQ(0.0, utl::change_percent(0, 0));
  EXPECT_DOUBLE_EQ(0.0, utl::change_percent(0, 100));
}

TEST(HealthMonitorUtil, PreservesSmallChangesInLargeCounters) {
  constexpr uint64_t maximum = std::numeric_limits<uint64_t>::max();
  EXPECT_GT(utl::change_percent(maximum - 1, maximum), 0.0);
  EXPECT_GT(utl::change_percent(maximum, maximum - 1), 0.0);
  EXPECT_LT(utl::change_percent(maximum - 1, maximum), 1.0);
  EXPECT_DOUBLE_EQ(100.0, utl::change_percent(maximum, 0));
}

}  // namespace
}  // namespace hm
