/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include <float.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <limits>

#include "m_string.h"
#include "mysql/strings/int2str.h"
#include "unittest/gunit/benchmark.h"

namespace m_string_unittest {

std::string HumanReadable(double bytes) {
  char buf[32];
  human_readable_num_bytes(buf, sizeof(buf), bytes);
  return buf;
}

TEST(MString, HumanReadableSize) {
  EXPECT_EQ("1", HumanReadable(1.0));
  EXPECT_EQ("1024", HumanReadable(1024.0));
  EXPECT_EQ("1K", HumanReadable(1024.1));
  EXPECT_EQ("1K", HumanReadable(1025.0));

  double data_size = 1025.0 * 1024;
  EXPECT_EQ("1M", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1G", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1T", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1P", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1E", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1Z", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1Y", HumanReadable(data_size));
  data_size *= 1024;
  EXPECT_EQ("1025Y", HumanReadable(data_size));
  data_size *= 1000;
  EXPECT_EQ("1025000Y", HumanReadable(data_size));
  data_size *= 1000;
  EXPECT_EQ("1025000000Y", HumanReadable(data_size));
  data_size *=
      static_cast<double>(std::numeric_limits<unsigned long long>::max());
  EXPECT_EQ("+INF", HumanReadable(data_size));

  // Various edge cases. We don't care much which way they round,
  // we just want them to not give nonsensical results such as “1024K”
  // for 1024.001.
  EXPECT_EQ("1023", HumanReadable(nextafter(1024.0, -DBL_MAX)));
  EXPECT_EQ("1K", HumanReadable(nextafter(1024.0, DBL_MAX)));

  double yotta = pow(1024.0, 8.0);
  EXPECT_EQ("9223372036854774784Y",
            HumanReadable(
                nextafter(static_cast<double>(LLONG_MAX) * yotta, -DBL_MAX)));
  EXPECT_EQ("9223372036854775808Y",
            HumanReadable(static_cast<double>(LLONG_MAX) * yotta));
  EXPECT_EQ("9223372036854777856Y",
            HumanReadable(
                nextafter(static_cast<double>(LLONG_MAX) * yotta, DBL_MAX)));

  EXPECT_EQ("18446744073709549568Y",
            HumanReadable(
                nextafter(static_cast<double>(ULLONG_MAX) * yotta, -DBL_MAX)));
  EXPECT_EQ("+INF", HumanReadable(static_cast<double>(ULLONG_MAX) * yotta));
  EXPECT_EQ("+INF", HumanReadable(nextafter(
                        static_cast<double>(ULLONG_MAX) * yotta, DBL_MAX)));
}

static void BM_longlong10_to_str(size_t num_iterations) {
  StopBenchmarkTiming();
  const int64_t value = 1234567890123456789;

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    char buffer[std::numeric_limits<int64_t>::digits10 + 2];
    longlong10_to_str(value, buffer, -10);
  }
}
BENCHMARK(BM_longlong10_to_str)

static void BM_longlong2str(size_t num_iterations) {
  StopBenchmarkTiming();
  const int64_t value = 1234567890123456789;

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    char buffer[100];
    longlong2str(value, buffer, -36);
  }
}
BENCHMARK(BM_longlong2str)

}  // namespace m_string_unittest
