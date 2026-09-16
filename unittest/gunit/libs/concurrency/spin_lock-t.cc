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
#include <algorithm>
#include <climits>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

#include "mysql/concurrency/spin_lock_mutex.h"
#include "mysql/concurrency/thread.h"

namespace mysql::concurrency {

// Basic test which verifies that SpinLockMutex allows only one thread to work
// on a specified resource
//
// R1. The Concurrency library shall provide optimized implementation of a
//     a spin lock mutex
//
// 1. Prepare producer thread, asynchronously copying data into synchronized
//    resource
// 2. Prepare consumer thread, asynchronously copying data from the synchronized
//    resource
// 3. Synchronize with threads
// 4. Check that consumed data is as expected (equal to the pattern)
TEST(Concurrency, SpinLockBasic) {
  // prepare data:
  std::size_t num_elems = 500000;
  std::vector<uint32_t> pattern(num_elems);

  using random_bytes_engine =
      std::independent_bits_engine<std::default_random_engine,
                                   CHAR_BIT * sizeof(uint32_t), uint32_t>;
  random_bytes_engine rbe;
  std::generate(pattern.begin(), pattern.end(), std::ref(rbe));

  // resource for which we implement a critical section
  std::vector<uint32_t> data(num_elems), consumed_data(num_elems);
  Spin_lock_mutex spin_mutex;

  /// thanks to this atomic variable we make sure that we start consumer after
  /// producer has taken the lock
  std::atomic<bool> producer_started{false};

  // create a producer
  concurrency::Thread producer_thread(
      [&](size_t) -> void {
        std::scoped_lock lock(spin_mutex);
        producer_started.store(true);
        std::size_t id = 0;
        for (const auto &elem : pattern) {
          data[id++] = elem;
        }
      },
      0);

  // create a consumer
  concurrency::Thread consumer_thread(
      [&](size_t) -> void {
        while (!producer_started.load()) {
          std::this_thread::yield();
        }
        std::scoped_lock lock(spin_mutex);
        std::size_t id = 0;
        for (const auto &elem : data) {
          consumed_data[id++] = elem;
        }
      },
      0);

  producer_thread.join();
  consumer_thread.join();

  EXPECT_TRUE(0 == std::memcmp(consumed_data.data(), pattern.data(),
                               num_elems * sizeof(uint32_t)));
}

}  // namespace mysql::concurrency
