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
#include <cstring>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry.h"
#include "mysql/scheduler/task_registry_multi.h"

using namespace std;

namespace mysql::scheduler {
namespace {

struct Object_type_1 {
  int a{0};
};

}  // namespace

TEST(TaskRegistryTester, Sanity) {
  using Task_registry_type = Task_registry<Task_id, Object_type_1>;
  Task_registry_type reg(Task_registry_type::default_capacity);
  ASSERT_TRUE(reg.register_entry(0, Object_type_1{5}));
  ASSERT_TRUE(reg.register_entry(1, Object_type_1{6}));
  ASSERT_TRUE(reg.register_entry(2, Object_type_1{7}));
  ASSERT_TRUE(reg.register_entry(3, Object_type_1{8}));
  ASSERT_FALSE(reg.register_entry(3, Object_type_1{8}));
  ASSERT_TRUE(reg.deactivate_entry(3));
  ASSERT_FALSE(reg.deactivate_entry(3));
  ASSERT_TRUE(reg.deactivate_entry(0));

  std::vector<int> res;
  std::vector<int> target{6, 7};
  auto print = [&res](auto &obj) -> auto{ res.push_back(obj.a); };
  reg.apply_on_active(print);

  auto capture = [](auto &obj, int &captured) -> auto{ captured = obj.a; };

  ASSERT_EQ(res.size(), target.size());
  auto target_it = target.begin();
  for (const auto &elem : res) {
    ASSERT_EQ(elem, *target_it++);
  }

  using namespace std::placeholders;

  int captured_value_task_1{0};
  std::ignore =
      reg.apply(1, std::bind(capture, _1, std::ref(captured_value_task_1)));
  ASSERT_EQ(captured_value_task_1, 6);
}

namespace {

struct Test_object {
  int value{0};
  Test_object() = default;
  Test_object(int v) : value(v) {}
};

}  // namespace

// Test basic functionality of Task_registry_multi: activation, deactivation,
// apply operations, and error handling for duplicate activations.
TEST(TaskRegistryMultiTester, Sanity) {
  using Task_registry_type = Task_registry_multi<Task_id, Test_object>;
  Task_registry_type reg(100);

  Task_id id1(0);
  Task_id id2(1);
  Task_id id3(2);

  ASSERT_TRUE(reg.activate(id1, Test_object{5}));
  ASSERT_TRUE(reg.activate(id2, Test_object{6}));
  ASSERT_TRUE(reg.activate(id3, Test_object{7}));

  // Try to activate again, should fail
  ASSERT_FALSE(reg.activate(id1, Test_object{10}));

  // Apply on active
  int captured = 0;
  ASSERT_TRUE(
      reg.apply(id1, [&captured](Test_object &obj) { captured = obj.value; }));
  ASSERT_EQ(captured, 5);

  // Apply on inactive
  ASSERT_FALSE(reg.apply(Task_id(99), [](Test_object &) {}));

  // Deactivate
  ASSERT_TRUE(reg.deactivate(id1));
  ASSERT_FALSE(reg.deactivate(id1));  // Already inactive

  // Apply on deactivated
  ASSERT_FALSE(reg.apply(id1, [](Test_object &) {}));
}

// Test edge cases: operations on non-existing tasks, reactivation after
// deactivation. Ensures robustness and correct behavior for invalid or repeated
// operations.
TEST(TaskRegistryMultiTester, CornerCases) {
  using Task_registry_type = Task_registry_multi<Task_id, Test_object>;
  Task_registry_type reg(10);

  Task_id id(0);

  // Deactivate non-existing
  ASSERT_FALSE(reg.deactivate(id));

  // Apply on non-existing
  ASSERT_FALSE(reg.apply(id, [](Test_object &) {}));

  // Activate, then deactivate, then activate again
  ASSERT_TRUE(reg.activate(id, Test_object{1}));
  ASSERT_TRUE(reg.deactivate(id));
  ASSERT_TRUE(reg.activate(id, Test_object{2}));

  int captured = 0;
  ASSERT_TRUE(
      reg.apply(id, [&captured](Test_object &obj) { captured = obj.value; }));
  ASSERT_EQ(captured, 2);
}

// Test hash collision handling by forcing multiple tasks into the same bucket.
// Verifies that Task_registry_multi correctly manages multiple entries per
// bucket and maintains isolation between tasks with different IDs.
TEST(TaskRegistryMultiTester, HashCollisions) {
  using Task_registry_type = Task_registry_multi<Task_id, Test_object>;
  Task_registry_type reg(1);  // Force all into same bucket

  Task_id id1(0);
  Task_id id2(1);
  Task_id id3(2);

  ASSERT_TRUE(reg.activate(id1, Test_object{10}));
  ASSERT_TRUE(reg.activate(id2, Test_object{20}));
  ASSERT_TRUE(reg.activate(id3, Test_object{30}));

  // All should be active
  int count = 0;
  auto counter = [&count](Test_object &) { ++count; };
  ASSERT_TRUE(reg.apply(id1, counter));
  ASSERT_TRUE(reg.apply(id2, counter));
  ASSERT_TRUE(reg.apply(id3, counter));
  ASSERT_EQ(count, 3);

  // Deactivate one
  ASSERT_TRUE(reg.deactivate(id2));
  count = 0;
  ASSERT_TRUE(reg.apply(id1, counter));
  ASSERT_FALSE(reg.apply(id2, counter));
  ASSERT_TRUE(reg.apply(id3, counter));
  ASSERT_EQ(count, 2);
}

// Test thread safety by running multiple threads performing activate, apply,
// and deactivate operations concurrently. Ensures no race conditions or data
// corruption under high concurrency, which is critical for production use.
TEST(TaskRegistryMultiTester, ConcurrentAccess) {
  using Task_registry_type = Task_registry_multi<Task_id, Test_object>;
  Task_registry_type reg(100);

  const int num_threads = 10;
  const int tasks_per_thread = 10;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  auto worker = [&reg, &success_count](int start_id) {
    for (int i = 0; i < tasks_per_thread; ++i) {
      Task_id id(start_id * tasks_per_thread + i);
      if (reg.activate(id, Test_object{i})) {
        ++success_count;
      }
      // Apply
      int captured = -1;
      if (reg.apply(id,
                    [&captured](Test_object &obj) { captured = obj.value; })) {
        ASSERT_EQ(captured, i);
      }
      // Deactivate
      ASSERT_TRUE(reg.deactivate(id));
    }
  };

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back(worker, t);
  }

  for (auto &th : threads) {
    th.join();
  }

  ASSERT_EQ(success_count, num_threads * tasks_per_thread);
}

// Test performance and scalability with a large number of tasks (10,000).
// Includes random access patterns to simulate real-world usage and verify
// that the registry handles high loads efficiently without errors.
TEST(TaskRegistryMultiTester, LargeScale) {
  using Task_registry_type = Task_registry_multi<Task_id, Test_object>;
  Task_registry_type reg(1000);

  const int num_tasks = 10000;
  std::vector<Task_id> ids;

  // Activate many tasks
  for (int i = 0; i < num_tasks; ++i) {
    Task_id id(i);
    ids.push_back(id);
    ASSERT_TRUE(reg.activate(id, Test_object{i}));
  }

  // Random apply
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, num_tasks - 1);

  for (int i = 0; i < 1000; ++i) {
    int idx = dis(gen);
    int captured = -1;
    ASSERT_TRUE(reg.apply(
        ids[idx], [&captured](Test_object &obj) { captured = obj.value; }));
    ASSERT_EQ(captured, idx);
  }

  // Deactivate all
  for (const auto &id : ids) {
    ASSERT_TRUE(reg.deactivate(id));
  }

  // Verify all inactive
  for (const auto &id : ids) {
    ASSERT_FALSE(reg.apply(id, [](Test_object &) {}));
  }
}

// Test Clock_lwm_registry: basic LWM calculation with registry.
TEST(ClockLwmRegistryTester, BasicLwm) {
  Clock_lwm_registry clock;

  // Initially LWM = 0
  ASSERT_EQ(clock.now(), 0);

  // Add time (subscribe task)
  ASSERT_TRUE(clock.add_time(Task_id(0), 1));

  // Tick task 1: LWM advances to 1
  ASSERT_TRUE(clock.tick(Task_id(0), 1));
  ASSERT_EQ(clock.now(), 1);

  // Add and tick task 2: LWM advances to 2
  ASSERT_TRUE(clock.add_time(Task_id(1), 2));
  ASSERT_TRUE(clock.tick(Task_id(1), 2));
  ASSERT_EQ(clock.now(), 2);
}

// Test concurrent ticks on Clock_lwm_registry.
TEST(ClockLwmRegistryTester, ConcurrentTicks) {
  Clock_lwm_registry clock;

  const int num_threads = 10;
  const int tasks_per_thread = 10;
  std::vector<std::thread> threads;
  std::atomic<int> completed{0};

  auto worker = [&clock, &completed](int start_id) {
    for (int i = 0; i < tasks_per_thread; ++i) {
      int id = start_id * tasks_per_thread + i;  // IDs from 0
      clock.add_time(Task_id(id), id);
      clock.tick(Task_id(id), id);
      ++completed;
    }
  };

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back(worker, t);
  }

  for (auto &th : threads) {
    th.join();
  }

  ASSERT_EQ(completed, num_threads * tasks_per_thread);
  // LWM should be the total number of tasks since they execute in order
  ASSERT_EQ(clock.now(), num_threads * tasks_per_thread);
}

}  // namespace mysql::scheduler
