/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <list>
#include <random>
#include <string>
#include <thread>

#include "secure_memory_pool.h"  // NOLINT(build/include_subdir)

namespace mysql_harness {
namespace {

TEST(TestSecureMemoryPool, allocation_deallocation) {
  constexpr std::size_t kThreadCount = 1;
  char id = '0';

  const auto test_scenario = [](char my_id) {
    SCOPED_TRACE("ID: " + std::string(1, my_id));

    constexpr std::size_t kLoopCount = 500;
    std::list<std::pair<char *, std::size_t>> allocated;
    auto &pool = SecureMemoryPool::get();

    std::mt19937 generator{std::random_device()()};
    std::uniform_int_distribution<std::size_t> bytes{1, 65};
    std::uniform_int_distribution<> long_living{0, 1};
    std::uniform_real_distribution<> operation{0.0, 1.0};

    const auto allocate = [&]() {
      const auto size = bytes(generator);
      const auto ptr = static_cast<char *>(pool.allocate(size));

      std::memset(ptr, my_id, size);

      if (long_living(generator)) {
        allocated.emplace_back(ptr, size);
      } else {
        allocated.emplace_front(ptr, size);
      }
    };

    const auto deallocate = [&]() {
      const auto memory = allocated.front();
      allocated.pop_front();

      std::size_t mismatched = 0;

      for (std::size_t i = 0; i < memory.second; ++i) {
        if (my_id != memory.first[i]) {
          ++mismatched;
        }
      }

      EXPECT_EQ(0, mismatched);

      pool.deallocate(memory.first, memory.second);
    };

    const auto random_operation = [&](double allocate_probability) {
      if (operation(generator) < allocate_probability) {
        allocate();
      } else if (!allocated.empty()) {
        deallocate();
      }
    };

    // just allocate
    for (std::size_t i = 0; i < kLoopCount; ++i) {
      allocate();
    }

    // allocate and deallocate, allocate is favoured
    for (std::size_t i = 0; i < kLoopCount; ++i) {
      random_operation(0.75);
    }

    // allocate and deallocate
    for (std::size_t i = 0; i < kLoopCount; ++i) {
      random_operation(0.5);
    }

    // allocate and deallocate, deallocate is favoured
    for (std::size_t i = 0; i < kLoopCount; ++i) {
      random_operation(0.25);
    }

    // deallocate the rest
    while (!allocated.empty()) {
      deallocate();
    }
  };

  const auto background_thread = [&test_scenario](char my_id) {
    try {
      test_scenario(my_id);
    } catch (const std::exception &e) {
      EXPECT_TRUE(false) << "ID: " << my_id << ", error: " << e.what();
    }
  };

  std::array<std::thread, kThreadCount> threads;

  for (auto &thread : threads) {
    thread = std::thread{background_thread, ++id};
  }

  // main thread also runs the test
  background_thread(++id);

  for (auto &thread : threads) {
    thread.join();
  }
}

}  // namespace
}  // namespace mysql_harness

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
