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
#include <string>
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

// --- try_lock_resource (non-blocking optimistic reserve, entry-based) ---

using Monitor = Resource_instance_monitor;

// Fast path: a reservation that fits is taken; once the resource is exhausted,
// a further non-blocking attempt fails immediately without waiting.
TEST_F(RplResourceMonitorTest, TryLockResourceFastPath) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("try_res", 10);
  Resource_entry *res = monitor.get_resource_entry("try_res");
  ASSERT_NE(res, nullptr);

  EXPECT_TRUE(Monitor::try_lock_resource(*res, 4));  // 10 -> 6
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 6));  // 6  -> 0
  // Nothing left and the request is smaller than the limit: no wait, just fail.
  EXPECT_FALSE(Monitor::try_lock_resource(*res, 1));

  Monitor::release_resource(*res, 10);
  // Fully free again: a fitting request succeeds.
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 10));
  Monitor::release_resource(*res, 10);
}

// A failed attempt rolls back and consumes nothing, so a later fitting request
// still succeeds against the unchanged available count.
TEST_F(RplResourceMonitorTest, TryLockResourceRollsBackOnMiss) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("try_res_rollback", 10);
  Resource_entry *res = monitor.get_resource_entry("try_res_rollback");
  ASSERT_NE(res, nullptr);

  EXPECT_TRUE(Monitor::try_lock_resource(*res, 3));  // 10 -> 7
  // 8 does not fit in the remaining 7, and 7 != limit: fail and roll back.
  EXPECT_FALSE(Monitor::try_lock_resource(*res, 8));  // stays 7
  // The rollback restored the 8, so the remaining 7 is still fully available.
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 7));  // 7 -> 0

  Monitor::release_resource(*res, 10);
}

// The "available == limit" rule admits a request even when it exceeds the
// limit, but only while the resource is fully free (the forward-progress
// guarantee for an oversized request).
TEST_F(RplResourceMonitorTest, TryLockResourceAdmitsAtLimit) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("try_res_limit", 5);
  Resource_entry *res = monitor.get_resource_entry("try_res_limit");
  ASSERT_NE(res, nullptr);

  // Fully free: a request larger than the limit is admitted (prev == limit).
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 8));
  // Now overshot (available is negative): a further request is refused.
  EXPECT_FALSE(Monitor::try_lock_resource(*res, 1));

  Monitor::release_resource(*res, 8);
  // Back to fully free: an exact-limit request is admitted.
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 5));
  Monitor::release_resource(*res, 5);
}

// The soft rule admits any amount while the resource is under its limit
// (available > 0), even one larger than what is available or than the limit,
// and then refuses at or over the limit until enough is released.
TEST_F(RplResourceMonitorTest, TryLockResourceSoftAdmitsWhileUnderLimit) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("soft_res", 10);
  Resource_entry *res = monitor.get_resource_entry("soft_res");
  ASSERT_NE(res, nullptr);

  // Under the limit: an amount larger than the limit is admitted.
  EXPECT_TRUE(Monitor::try_lock_resource_soft(*res, 25));  // 10 -> -15
  EXPECT_EQ(res->m_available.load(), -15);
  // Over the limit: refused, and the count is unchanged.
  EXPECT_FALSE(Monitor::try_lock_resource_soft(*res, 1));
  EXPECT_EQ(res->m_available.load(), -15);

  // Released down to exactly the limit (available == 0): still refused.
  Monitor::release_resource(*res, 15);  // -15 -> 0
  EXPECT_FALSE(Monitor::try_lock_resource_soft(*res, 1));
  EXPECT_EQ(res->m_available.load(), 0);

  // One unit under the limit: admitted again, whatever the amount.
  Monitor::release_resource(*res, 1);                     // 0 -> 1
  EXPECT_TRUE(Monitor::try_lock_resource_soft(*res, 7));  // 1 -> -6
  Monitor::release_resource(*res, 16);                    // -6 -> 10
  EXPECT_EQ(res->m_available.load(), 10);
}

// Concurrent soft reservations: each one that succeeds starts from available
// > 0, so available never drops below 1 - amount, and the count stays exact
// after all releases (no lost updates from the compare-and-swap loop).
TEST_F(RplResourceMonitorTest, TryLockResourceSoftConcurrent) {
  constexpr int kThreads = 8;
  constexpr int kIterations = 20000;
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("soft_res_mt", 1000);
  Resource_entry *res = monitor.get_resource_entry("soft_res_mt");
  ASSERT_NE(res, nullptr);

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([res] {
      for (int i = 0; i < kIterations; ++i) {
        if (Monitor::try_lock_resource_soft(*res, 300)) {
          EXPECT_GE(res->m_available.load(), 1 - 300);
          Monitor::release_resource(*res, 300);
        }
      }
    });
  }
  for (auto &th : threads) th.join();
  EXPECT_EQ(res->m_available.load(), 1000);
}

// try_lock_resource must not disturb a separately registered resource: the
// reference declared_channel_memory-style path (lock/release) still behaves as
// before on a second resource alongside it.
TEST_F(RplResourceMonitorTest, TryLockResourceIndependentOfOtherResources) {
  auto &monitor = Resource_monitor::get(0);
  monitor.register_resource("try_res_a", 4);
  monitor.register_resource("try_res_b", 4);
  Resource_entry *a = monitor.get_resource_entry("try_res_a");
  ASSERT_NE(a, nullptr);

  EXPECT_TRUE(Monitor::try_lock_resource(*a, 4));  // a exhausted
  EXPECT_FALSE(Monitor::try_lock_resource(*a, 1));
  // b is untouched by a's reservation and still lockable in full.
  EXPECT_TRUE(monitor.lock_resource("try_res_b", 4));

  Monitor::release_resource(*a, 4);
  monitor.release_resource("try_res_b", 4);
}

// An entry pointer stays valid while other names are registered on the same
// monitor, even many of them (the map may rehash): only re-registering the
// same name replaces an entry. This is what lets a thread keep using its entry
// while another thread registers its own resources.
TEST_F(RplResourceMonitorTest, EntryStaysValidAfterOtherRegistrations) {
  auto &monitor = Resource_monitor::get(2);
  monitor.register_resource("kept_res", 10);
  Resource_entry *res = monitor.get_resource_entry("kept_res");
  ASSERT_NE(res, nullptr);
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 3));  // 10 -> 7

  for (int i = 0; i < 1000; ++i)
    monitor.register_resource("other_res_" + std::to_string(i), 1);

  // Same entry, same state: the reservation is still counted.
  EXPECT_EQ(res->m_available.load(), 7);
  EXPECT_TRUE(Monitor::try_lock_resource(*res, 7));  // 7 -> 0
  EXPECT_FALSE(Monitor::try_lock_resource(*res, 1));
  Monitor::release_resource(*res, 10);
  // The name-based path reaches the same entry.
  EXPECT_TRUE(monitor.lock_resource("kept_res", 10));
  EXPECT_EQ(res->m_available.load(), 0);
  monitor.release_resource("kept_res", 10);
}

// A name that was never registered has no entry.
TEST_F(RplResourceMonitorTest, GetResourceEntryOfUnknownNameIsNull) {
  auto &monitor = Resource_monitor::get(2);
  EXPECT_EQ(monitor.get_resource_entry("never_registered_res"), nullptr);
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
