/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
#include <random>
#include <string>
#include "unhex.h"
#include "unittest/gunit/benchmark.h"

namespace {
std::string random_string(std::string::size_type length) {
  const std::string alphanum = "0123456789abcdefABCDEF";
  std::mt19937 rng{42};
  std::uniform_int_distribution<std::string::size_type> dist(
      0, alphanum.size() - 1);

  std::string result;
  result.reserve(length);
  for (std::string::size_type i = 0; i < length; i++) {
    result += alphanum[dist(rng)];
  }
  return result;
}

void BM_Unhex(size_t num_iterations, size_t string_size) {
  StopBenchmarkTiming();
  auto s = random_string(string_size);
  std::string output(string_size * 2, '\0');

  StartBenchmarkTiming();
  for (size_t n = 0; n < num_iterations; n++) {
    (void)unhex(s.data(), s.data() + s.size(), &output[0]);
  }
  StopBenchmarkTiming();
  SetBytesProcessed(num_iterations * s.size());
}
}  // namespace

auto UnhexLookup4k = [](size_t num_iterations) {
  BM_Unhex(num_iterations, 4 * 1024);
};

auto UnhexLookup512k = [](size_t num_iterations) {
  BM_Unhex(num_iterations, 512 * 1024);
};

auto UnhexLookup1M = [](size_t num_iterations) {
  BM_Unhex(num_iterations, 1024 * 1024);
};

auto UnhexLookup4M = [](size_t num_iterations) {
  BM_Unhex(num_iterations, 4 * 1024 * 1024);
};

BENCHMARK(UnhexLookup4k)
BENCHMARK(UnhexLookup512k)
BENCHMARK(UnhexLookup1M)
BENCHMARK(UnhexLookup4M)
