// Copyright (c) 2026, Oracle and/or its affiliates.
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

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "mysql/scheduler/sharded_counter.h"

using namespace mysql::scheduler;

TEST(ConcurrentCounterTest, MultiThreadedWrite) {
  constexpr std::size_t max_threads = 20;
  constexpr std::size_t num_threads = 10;

  Sharded_counter counter;
  counter.init(max_threads);

  std::vector<std::thread> threads;

  for (unsigned int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&counter, i] { counter.add(1, i); });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  for (unsigned int i = 0; i < num_threads; ++i) {
    EXPECT_EQ(counter.get(i), 1);
  }
  EXPECT_EQ(counter.get(), num_threads);
}

TEST(ConcurrentCounterTest, MultiThreadedWriteMultipleTimes) {
  constexpr std::size_t max_threads = 20;
  constexpr std::size_t num_threads = 10;
  constexpr std::size_t num_writes = 5;

  Sharded_counter counter;
  counter.init(max_threads);

  std::vector<std::thread> threads;

  for (unsigned int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&counter, i] {
      for (std::size_t j = 0; j < num_writes; ++j) {
        counter.add(1, i);
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  for (unsigned int i = 0; i < num_threads; ++i) {
    EXPECT_EQ(counter.get(i), num_writes);
  }
  EXPECT_EQ(counter.get(), num_threads * num_writes);
}
