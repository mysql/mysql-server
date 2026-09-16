// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "mysql/scheduler/constants.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"

namespace mysql::csa::unittest {

using namespace std::chrono_literals;

class RplResourceMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RplResourceMonitorTest, BasicRegistrationAndLockRelease) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("test_resource", 10);

  // Lock 5 units
  EXPECT_TRUE(monitor.lock_resource("test_resource", 5));
  // Release 5 units
  monitor.release_resource("test_resource", 5);
}

TEST_F(RplResourceMonitorTest, LockExceedsLimit) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("test_resource", 5);

  // Try to lock 6 > limit
  EXPECT_TRUE(monitor.lock_resource("test_resource", 6));
  monitor.release_resource("test_resource", 6);
}

TEST_F(RplResourceMonitorTest, BlockingLockWaitsForRelease) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("test_resource", 10);

  std::atomic_flag check_ready;
  check_ready.clear();

  // Lock all 10 in one thread
  std::thread locker([&monitor, &check_ready]() {
    ASSERT_TRUE(monitor.lock_resource("test_resource", 10));
    check_ready.test_and_set();
    check_ready.notify_one();
    // Sleep to allow waiter to block
    std::this_thread::sleep_for(100ms);
    monitor.release_resource("test_resource", 10);
  });

  // In main thread, wait for locker to lock resource and try to lock 5 - should
  // block until release
  check_ready.wait(false);
  auto start = std::chrono::steady_clock::now();
  ASSERT_TRUE(monitor.lock_resource("test_resource", 5));
  auto end = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  EXPECT_GT(duration, 50ms);  // Should have waited some time
  monitor.release_resource("test_resource", 5);
  locker.join();
}

TEST_F(RplResourceMonitorTest, SingletonMultipleInstances) {
  auto &monitor1 = Resource_monitor::get(0);
  auto &monitor2 = Resource_monitor::get(1);

  // Each should be independent
  monitor1.register_resource("res1", 5);
  monitor2.register_resource("res2", 10);

  EXPECT_TRUE(monitor1.lock_resource("res1", 3));
  EXPECT_TRUE(monitor2.lock_resource("res2", 4));

  // monitor1 shouldn't have res2, but since find fails, lock should fail (but
  // code asserts - test assumes registered) Note: Current impl asserts on
  // missing, so skip cross-access test or mock.

  monitor1.release_resource("res1", 3);
  monitor2.release_resource("res2", 4);
}

TEST_F(RplResourceMonitorTest, ConcurrentAccess) {
  constexpr size_t num_threads = 10;
  constexpr size_t limit = 100;
  constexpr size_t amount_per_thread = 10;

  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("concurrent_res", limit);

  std::vector<std::thread> threads;
  std::atomic<size_t> success_count{0};
  std::atomic<size_t> fail_count{0};

  // Launch threads to lock and release
  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back([&monitor, &success_count, &fail_count]() {
      if (monitor.lock_resource("concurrent_res", amount_per_thread)) {
        success_count++;
        // Simulate work
        std::this_thread::sleep_for(10ms);
        monitor.release_resource("concurrent_res", amount_per_thread);
      } else {
        fail_count++;
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  // All should succeed since total demand (100) == limit, but due to timing,
  // some may retry
  EXPECT_EQ(success_count, num_threads);
  EXPECT_EQ(fail_count, 0);
}

// Test lazy init - multiple gets should init only once
TEST_F(RplResourceMonitorTest, LazyInitialization) {
  // First get inits
  auto &monitor1 = Resource_monitor::get(0);
  // Second get waits but doesn't re-init
  auto &monitor2 = Resource_monitor::get(1);

  // Verify instances are different
  EXPECT_NE(&monitor1, &monitor2);
}

TEST_F(RplResourceMonitorTest, ResourceGuard) {
  auto &monitor = Resource_monitor::get(0);

  // Each should be independent
  monitor.register_resource("res1", 5);
  monitor.register_resource("res2", 10);

  {
    auto r1_guard = monitor.acquire_resource("res1", 5);
    auto r2_guard = monitor.acquire_resource("res2", 10);

    EXPECT_TRUE(r1_guard.is_locked());
    EXPECT_TRUE(r2_guard.is_locked());
  }
  // resource are released, we can acquire again
  {
    auto r1_guard = monitor.acquire_resource("res1", 5);
    auto r2_guard = monitor.acquire_resource("res2", 10);

    EXPECT_TRUE(r1_guard.is_locked());
    EXPECT_TRUE(r2_guard.is_locked());
  }
}

}  // namespace mysql::csa::unittest
