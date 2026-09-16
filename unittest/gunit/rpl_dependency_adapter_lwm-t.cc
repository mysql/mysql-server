/*
  Copyright (c) 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>

#include "libs/mysql/scheduler/clock_lwm_registry.h"
#include "sql/changestreams/apply/scheduler/dependency_adapter.h"
#include "sql/changestreams/apply/scheduler/dependency_adapter_lwm.h"

namespace mysql::csa_unittest {

using Dependency_adapter_lwm = mysql::csa::Dependency_adapter_lwm;
using Task_id = mysql::csa::Dependency_adapter::Task_id;
using Clock_delay = mysql::csa::Dependency_adapter::Clock_delay;

class RplDependencyAdapterLwmTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RplDependencyAdapterLwmTest, ChainedDependencies) {
  Dependency_adapter_lwm adapter;
  std::optional<uint64_t> delay{0};
  std::optional<Task_id> dep_task_id;
  // Chain the calls to simulate state
  // 1. solve(0,1,0) -> exec now
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(0), 1LL, 0LL);
  EXPECT_EQ(delay.value(), 0LL);

  // 2. solve(1,2,1)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(1), 2LL, 1LL);
  EXPECT_EQ(delay.value(), 1LL);

  // 3. solve(2,3,1)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(2), 3LL, 1LL);
  EXPECT_EQ(delay.value(), 1LL);

  // 4. solve(3,4,3)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(3), 4LL, 3LL);
  EXPECT_EQ(delay.value(), 3LL);

  // 5. solve(4,5,1)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(4), 5LL, 1LL);
  EXPECT_EQ(delay.value(), 1LL);
  EXPECT_EQ(adapter.size(), 5);

  // 6. solve(5,6,0) : Map clear on barrier - invalid commit parent
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(5), 6LL, 0LL);
  EXPECT_EQ(delay.value(), 5LL);
  EXPECT_EQ(adapter.size(), 1);

  // 7. solve(6,7,1): after the barrier, keep the lowest LWM possible
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(6), 7LL, 1LL);
  EXPECT_EQ(delay.value(), 5LL);
  // don't clear map on missing entry
  EXPECT_EQ(adapter.size(), 2);

  // 8. solve(7,8,6) : update normally
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(7), 8LL, 6LL);
  EXPECT_EQ(delay.value(), 6LL);

  // 9. solve(8,9,6) : keep lowest LWM
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(8), 9LL, 1LL);
  EXPECT_EQ(delay.value(), 5LL);
  EXPECT_EQ(adapter.size(), 4);

  // 9. reset source - seq = 1
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(9), 1LL, 0LL);
  EXPECT_EQ(delay.value(), 9LL);
  EXPECT_EQ(adapter.size(), 1);

  // 9. keep correct seq mapping after sequence number reset
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(10), 2LL, 1LL);
  EXPECT_EQ(delay.value(), 10LL);
  EXPECT_EQ(adapter.size(), 2);
}

TEST_F(RplDependencyAdapterLwmTest, WrapAroundHandling) {
  Dependency_adapter_lwm adapter;
  std::optional<uint64_t> delay{0};
  std::optional<Task_id> dep_task_id;

  // Simulate near-wrap task_ids
  uint64_t near_max = UINT64_MAX - 5;
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(near_max - 4), 1LL, 0LL);
  EXPECT_EQ(delay.value(), near_max - 4);

  // Continue to max
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(near_max), 2LL, 1LL);
  EXPECT_EQ(delay.value(), near_max - 3);  // parent +1

  // Simulate wrap to 0
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(0), 3LL, 0LL);
  // Detect wrap, reset barrier to 0, delay=0 (immediate post-wrap)
  EXPECT_EQ(delay.value(), 0ULL);
  EXPECT_EQ(adapter.size(), 1);  // map cleared and new insert

  // Next task_id=1, commit_parent=3 (seq_num=3 mapped to task_id=0)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(1), 4LL, 3LL);
  EXPECT_EQ(delay.value(), 1ULL);  // parent (0) +1 =1
  EXPECT_EQ(adapter.size(), 2);

  // Barrier post-wrap
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(2), 5LL, 0LL);
  EXPECT_EQ(delay.value(), 2ULL);  // barrier=2
  EXPECT_EQ(adapter.size(), 1);    // cleared
}

// Integration test: LWM clock + adapter
void IntegrationTestBody(Dependency_adapter_lwm &adapter, auto &clock) {
  std::optional<uint64_t> delay{0};
  std::optional<Task_id> dep_task_id;

  // Basic chained dependencies
  // Task 0: delay=0, ready (LWM=0 >=0)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(0), 1LL, 0LL);
  EXPECT_EQ(delay.value(), 0ULL);

  clock.add_time(Task_id(0), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());

  // Tick 0, LWM=1
  EXPECT_TRUE(clock.tick(Task_id(0), 0ULL));
  EXPECT_EQ(clock.now(), 1ULL);

  // Task 1: delay=1, ready (1 >=1)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(1), 2LL, 1LL);
  EXPECT_EQ(delay.value(), 1ULL);

  clock.add_time(Task_id(1), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());

  // Tick 1, LWM=2
  EXPECT_TRUE(clock.tick(Task_id(1), 1ULL));
  EXPECT_EQ(clock.now(), 2ULL);

  // Task 2: delay=1, ready (2 >1)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(2), 3LL, 1LL);
  EXPECT_EQ(delay.value(), 1ULL);

  clock.add_time(Task_id(2), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());

  // Tick 2, LWM=3
  EXPECT_TRUE(clock.tick(Task_id(2), 1ULL));
  EXPECT_EQ(clock.now(), 3ULL);

  // Barrier task 3: delay=3, ready (3>=3)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(3), 4LL, 0LL);
  EXPECT_EQ(delay.value(), 3ULL);

  clock.add_time(Task_id(3), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());

  // Out-of-order task 4: solve and add_time, then tick early
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(4), 5LL, 4LL);
  EXPECT_EQ(delay.value(), 4ULL);
  clock.add_time(Task_id(4), delay.value());
  clock.tick(Task_id(4), 3ULL);
  EXPECT_EQ(clock.now(), 3ULL);  // still 3, no advance

  // Tick 3, advances to 4, then to 5 since 4 finished out-of-order
  EXPECT_TRUE(clock.tick(Task_id(3), 3ULL));
  EXPECT_EQ(clock.now(), 5ULL);

  // Task 5: delay=5, ready (LWM=5 >=5)
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(5), 6LL, 5LL);
  EXPECT_EQ(delay.value(), 5ULL);

  clock.add_time(Task_id(5), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());

  // Wrap-around integration
  uint64_t near_max = UINT64_MAX - 2;
  adapter.solve(Task_id(near_max), 100LL, 0LL);  // high max
  clock.test_set_current_lwm(near_max);
  EXPECT_EQ(clock.now(), near_max);

  // Wrap task: adapter resets delay=0, tick detects wrap, resets LWM=0,
  // advances to 1
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(0), 101LL, 0LL);
  EXPECT_EQ(delay.value(), 0ULL);

  clock.add_time(Task_id(0), delay.value());

  EXPECT_TRUE(clock.tick(
      Task_id(0), 0ULL));  // detect wrap, reset LWM=0, tid=0==0, advance to 1
  EXPECT_EQ(clock.now(), 1ULL);
  EXPECT_TRUE(clock.now() >= delay.value());  // 1 > 0

  // Next: delay=1, ready
  std::tie(delay, dep_task_id) = adapter.solve(Task_id(1), 102LL, 101LL);
  EXPECT_EQ(delay.value(), 1ULL);

  clock.add_time(Task_id(1), delay.value());

  EXPECT_TRUE(clock.now() >= delay.value());
}

TEST_F(RplDependencyAdapterLwmTest, IntegrationWithClockLwmRegistry) {
  using mysql::scheduler::Clock_lwm_registry;
  Dependency_adapter_lwm adapter;
  Clock_lwm_registry clock(8192);  // default capacity
  IntegrationTestBody(adapter, clock);
}

}  // namespace mysql::csa_unittest
