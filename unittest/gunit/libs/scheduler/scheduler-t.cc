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
#include <cstring>
#include <random>
#include <thread>

#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/dependency_tracker_single_predecessor_example.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_sequencer.h"
#include "sql/changestreams/apply/scheduler/dependency_adapter_lwm.h"

using namespace std;

namespace mysql::scheduler {
namespace {

using Clock_type = std::chrono::system_clock;
using Test_point_type = Clock_type::time_point;

template <std::size_t sleep_duration_ms>
struct Task_delayed {
  void operator()([[maybe_unused]] unsigned int thread_id) {
    auto start = std::chrono::system_clock::now();
    std::chrono::milliseconds elapsed;
    do {
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - start);
    } while (static_cast<std::size_t>(elapsed.count()) < sleep_duration_ms);
  }
};

}  // namespace

TEST(Scheduler, LwmRegistryWithDependencyTracker) {
  // Test Scheduler with Clock_lwm_registry and
  // Dependency_tracker_single_predecessor.
  // Confirming that dependencies are correctly enforced

  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Dependency_tracker_ptr dep(new Dependency_tracker_single_predecessor());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);
  Task_sequencer gen;

  std::vector<int> execution_order;
  std::mutex mutex_order;

  auto task_func = [&mutex_order, &execution_order]([[maybe_unused]] int th_id,
                                                    int id) {
    std::lock_guard<std::mutex> lock(mutex_order);
    execution_order.push_back(id);
    return 0;
  };

  // Task A (id 0)
  auto task_a_id = gen.next_id();
  scheduler.enqueue(schedule_factory.create(task_a_id, 0), task_func, 0);

  // Task B (id 1) depends on Task A
  auto task_b_id = gen.next_id();
  scheduler.enqueue_after(task_a_id, schedule_factory.create(task_b_id, 0),
                          task_func, 1);

  // Task C (id 2) depends on Task B
  auto task_c_id = gen.next_id();
  scheduler.enqueue_after(task_b_id, schedule_factory.create(task_c_id, 0),
                          task_func, 2);

  // Synchronize and check execution order
  scheduler.synchronize();

  // Verify that tasks executed in dependency order: A, B, C
  ASSERT_EQ(execution_order.size(), 3);
  EXPECT_EQ(execution_order[0], 0);  // Task A first
  EXPECT_EQ(execution_order[1], 1);  // Task B after A
  EXPECT_EQ(execution_order[2], 2);  // Task C after B
}

TEST(Scheduler, DestructorsCheck) {
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Dependency_tracker_ptr dep(new Dependency_tracker_single_predecessor());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);
  Task_delayed<200> task1;
  Task_sequencer gen;
  auto task_id = gen.next_id();
  std::ignore =
      scheduler.enqueue(schedule_factory.create(task_id, 0), std::move(task1));
}

namespace {

int task_parameters_global_variable{0};
std::atomic_flag task_parameters_global_variable_updated = ATOMIC_FLAG_INIT;
std::atomic_flag task_parameters_read_ready = ATOMIC_FLAG_INIT;
int task_parameters_read_value{0};

struct Struct_task_parameters {
  template <typename Tid, typename T>
  int operator()([[maybe_unused]] Tid &&thread_id, T &&variable) {
    task_parameters_global_variable_updated.wait(false);
    task_parameters_read_value = variable;
    task_parameters_read_ready.test_and_set();
    task_parameters_read_ready.notify_one();
    return 0;
  }
};

void test_task_parameters_main(int idx) {
  // clear global variables
  task_parameters_global_variable = 0;
  task_parameters_global_variable_updated.clear();
  task_parameters_read_ready.clear();
  task_parameters_read_value = 0;

  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Dependency_tracker_ptr dep(new Dependency_tracker_single_predecessor());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Struct_task_parameters task_0;
  std::size_t delay = 0;
  Schedule_factory schedule_factory(clock);
  Task_sequencer gen;
  ASSERT_TRUE(scheduler.enqueue(schedule_factory.create(gen.next_id(), delay),
                                task_0, task_parameters_global_variable));
  task_parameters_global_variable = idx;
  task_parameters_global_variable_updated.test_and_set();
  task_parameters_global_variable_updated.notify_one();
  task_parameters_read_ready.wait(false);
  ASSERT_EQ(task_parameters_read_value, idx);
  scheduler.synchronize();
}

}  // namespace

TEST(Scheduler, TaskParameters) {
  int test_num = 500;
  for (int idx = 0; idx < test_num; ++idx) {
    test_task_parameters_main(idx);
  }
}

TEST(Scheduler, AtomicFlag) {
  int test_num = 5000;
  for (int idx = 0; idx < test_num; ++idx) {
    // clear global variables
    task_parameters_global_variable = 0;
    task_parameters_global_variable_updated.clear();
    task_parameters_read_ready.clear();
    task_parameters_read_value = 0;
    Struct_task_parameters task_0;
    auto t1 =
        std::thread{task_0, 0U, std::ref(task_parameters_global_variable)};
    task_parameters_global_variable = idx;
    task_parameters_global_variable_updated.test_and_set();
    task_parameters_global_variable_updated.notify_one();
    task_parameters_read_ready.wait(false);
    ASSERT_EQ(task_parameters_read_value, idx);
    t1.join();
  }
}

namespace {

struct Task_faulty {
  Task_faulty() {}
  bool is_error() const { return m_is_error; }
  void operator()(unsigned int) {
    std::random_device global_random_dev;
    std::mt19937 global_rng(global_random_dev());
    std::uniform_int_distribution<std::mt19937::result_type> global_dist(0,
                                                                         100);
    auto random_number = global_dist(global_rng);
    if (random_number > 98) {
      m_is_error.store(true);
    }
    ++m_check;
  }
  /// Used to check if all tasks executed
  std::atomic<std::size_t> m_check{0};
  std::atomic<bool> m_is_error{false};
};

void scheduler_test_error_handling(
    std::size_t iterations,
    std::size_t worker_pool_size = std::thread::hardware_concurrency()) {
  std::ignore = Statistics_map::init_statistics(0);
  Scheduler_clock_ptr local_clock = std::make_shared<Clock_lwm_registry>();
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(worker_pool_size);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, local_clock, std::move(dep));
  Task_faulty task1;
  Schedule_factory schedule_factory(local_clock);
  Task_sequencer gen;

  for (std::size_t it = 0; it < iterations; ++it) {
    ASSERT_TRUE(scheduler.enqueue(schedule_factory.create(gen.next_id(), it),
                                  std::ref(task1)));
  }
  scheduler.synchronize();
}

}  // namespace

TEST(Scheduler, ErrorHandling) {
  int test_num = 100;
  for (int idx = 0; idx < test_num; ++idx) {
    scheduler_test_error_handling(100);
  }
}

TEST(DependencyTracker, CornerCases) {
  Dependency_tracker_single_predecessor dep;
  Task_sequencer gen;

  // Test activating tasks
  Task_id task1 = gen.next_id();
  Task_id task2 = gen.next_id();
  Task_id task3 = gen.next_id();

  EXPECT_TRUE(dep.activate_task(task1));
  EXPECT_TRUE(dep.activate_task(task2));
  EXPECT_TRUE(dep.activate_task(task3));

  // Test check_ready on tasks with no dependencies
  EXPECT_TRUE(dep.check_ready(task1));
  EXPECT_TRUE(dep.check_ready(task2));
  EXPECT_TRUE(dep.check_ready(task3));

  // Test adding dependency
  EXPECT_TRUE(dep.add_dependency(task1, task2));

  // Now task2 should not be ready
  EXPECT_TRUE(dep.check_ready(task1));   // still ready
  EXPECT_FALSE(dep.check_ready(task2));  // not ready

  // Add another dependency: task2 -> task3
  EXPECT_TRUE(dep.add_dependency(task2, task3));
  EXPECT_FALSE(dep.check_ready(task3));  // not ready

  // Mark task1 as finished
  auto newly_ready = dep.mark_dependency_met(task1, true);
  EXPECT_EQ(newly_ready.size(), 1);
  EXPECT_EQ(newly_ready[0], task2);

  // Now task2 should be ready
  EXPECT_TRUE(dep.check_ready(task2));
  EXPECT_FALSE(dep.check_ready(task3));  // task3 still not ready

  // Mark task2 as finished
  newly_ready = dep.mark_dependency_met(task2, true);
  EXPECT_EQ(newly_ready.size(), 1);
  EXPECT_EQ(newly_ready[0], task3);

  // Now task3 should be ready
  EXPECT_TRUE(dep.check_ready(task3));

  // Test get_successors (all cleared when finished)
  std::vector<Task_id> succ1 = dep.get_successors(task1);
  EXPECT_TRUE(succ1.empty());

  std::vector<Task_id> succ2 = dep.get_successors(task2);
  EXPECT_TRUE(succ2.empty());

  std::vector<Task_id> succ3 = dep.get_successors(task3);
  EXPECT_TRUE(succ3.empty());

  newly_ready = dep.mark_dependency_met(task3, true);
  EXPECT_TRUE(newly_ready.empty());  // no successors added

  // Test adding dependency on finished task
  Task_id task4 = gen.next_id();
  EXPECT_TRUE(dep.activate_task(task4));
  EXPECT_TRUE(dep.add_dependency(
      task3, task4));  // task3 is finished, dependency not added
  EXPECT_TRUE(dep.check_ready(task4));  // no predecessor set

  // Test check_ready on non-existent task
  Task_id nonexistent = gen.next_id();
  EXPECT_TRUE(dep.check_ready(nonexistent));  // should be true since no entry

  // Test add_dependency with non-existent predecessor
  Task_id task5 = gen.next_id();
  EXPECT_TRUE(dep.activate_task(task5));
  EXPECT_TRUE(dep.add_dependency(nonexistent,
                                 task5));  // should return true but not add
  EXPECT_TRUE(dep.check_ready(task5));     // still ready

  // Test multiple successors
  Task_id task6 = gen.next_id();
  Task_id task7 = gen.next_id();
  EXPECT_TRUE(dep.activate_task(task6));
  EXPECT_TRUE(dep.activate_task(task7));

  // task6 depends on task1 (already finished)
  EXPECT_TRUE(dep.add_dependency(task1, task6));
  EXPECT_TRUE(dep.check_ready(task6));  // task1 finished

  // task7 also depends on task1
  EXPECT_TRUE(dep.add_dependency(task1, task7));
  EXPECT_TRUE(dep.check_ready(task7));  // task1 finished

  // Check successors of task1 (cleared when finished)
  succ1 = dep.get_successors(task1);
  EXPECT_TRUE(succ1.empty());
}

TEST(DependencyTracker, ConcurrentAccess) {
  // Test concurrent access to Dependency_tracker_single_predecessor.
  // This test verifies thread safety and correctness when multiple threads
  // simultaneously perform operations on the dependency tracker.
  // It's crucial for the scheduler's reliability in multi-threaded environments
  // where tasks may be enqueued, dependencies added, and tasks completed
  // concurrently by different worker threads.

  Dependency_tracker_single_predecessor dep;

  const int num_threads = 10;  // Number of concurrent threads
  const int operations_per_thread =
      100;  // Operations per thread for stress testing

  std::vector<std::thread> threads;
  std::atomic<int> errors{0};  // Atomic counter for thread-safe error tracking

  // Worker function executed by each thread
  auto worker = [&](int thread_id) {
    for (int i = 0; i < operations_per_thread; ++i) {
      try {
        // Generate unique task IDs for this operation, spaced to avoid
        // collisions
        Task_id task1(thread_id * 1000000LL + i * 2);
        Task_id task2(thread_id * 1000000LL + i * 2 + 1);

        // Step 1: Activate both tasks in the dependency tracker
        if (!dep.activate_task(task1) || !dep.activate_task(task2)) {
          ++errors;  // Task activation should succeed for new tasks
          continue;
        }

        // Step 2: Establish dependency (task1 must complete before task2)
        if (!dep.add_dependency(task1, task2)) {
          ++errors;  // Dependency addition should succeed
          continue;
        }

        // Step 3: Verify initial readiness states
        // task1 should be ready (no dependencies), task2 should not be ready
        if (!dep.check_ready(task1) || dep.check_ready(task2)) {
          ++errors;  // Readiness check failed
          continue;
        }

        // Step 4: Mark task1 as finished
        auto newly_ready = dep.mark_dependency_met(task1, true);
        if (newly_ready.size() != 1 || newly_ready[0] != task2) {
          ++errors;  // Should return task2 as newly ready
          continue;
        }

        // Step 5: Verify task2 is now ready after task1 completion
        if (!dep.check_ready(task2)) {
          ++errors;  // task2 should be ready now
          continue;
        }

        // Step 6: Mark task2 as finished
        newly_ready = dep.mark_dependency_met(task2, true);
        if (!newly_ready.empty()) {
          ++errors;  // task2 has no successors, so should return empty
          continue;
        }

      } catch (...) {
        ++errors;  // Catch any unexpected exceptions
      }
    }
  };

  // Launch worker threads
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back(worker, t);
  }

  // Wait for all threads to complete
  for (auto &th : threads) {
    th.join();
  }

  // Verify no errors occurred during concurrent operations
  EXPECT_EQ(errors.load(), 0);
}

TEST(DependencyTracker, HashCollisions) {
  // Test dependency tracking with hash collisions.
  // This test simulates tasks that hash to the same bucket to ensure
  // correctness even when multiple tasks share the same bucket for their
  // dependencies

  // Use a small capacity to increase collision likelihood
  Dependency_tracker_single_predecessor dep;
  // Assume we can access internal registry, but since it's private,
  // we'll use Task_ids that are multiples apart by a large number.

  // Create tasks with IDs that may collide (depending on capacity)
  // Since capacity is 16384, use IDs 0, 16384, 32768, etc.
  std::vector<Task_id> tasks;
  const int num_tasks = 10;
  for (int i = 0; i < num_tasks; ++i) {
    tasks.push_back(
        Task_id(i * 16384ULL));  // Force collisions if capacity >=16384
    EXPECT_TRUE(dep.activate_task(tasks[i]));
  }

  // Add dependencies in a chain: task0 -> task1 -> task2 -> ... -> task9
  for (int i = 0; i < num_tasks - 1; ++i) {
    EXPECT_TRUE(dep.add_dependency(tasks[i], tasks[i + 1]));
  }

  // Verify initial readiness: only task0 should be ready
  for (int i = 0; i < num_tasks; ++i) {
    if (i == 0) {
      EXPECT_TRUE(dep.check_ready(tasks[i]));
    } else {
      EXPECT_FALSE(dep.check_ready(tasks[i]));
    }
  }

  // Mark tasks as finished one by one and check newly ready tasks
  for (int i = 0; i < num_tasks - 1; ++i) {
    auto newly_ready = dep.mark_dependency_met(tasks[i], true);
    EXPECT_EQ(newly_ready.size(), 1);
    EXPECT_EQ(newly_ready[0], tasks[i + 1]);
    // Now task i+1 should be ready
    EXPECT_TRUE(dep.check_ready(tasks[i + 1]));
  }

  // Mark the last task as finished
  auto newly_ready = dep.mark_dependency_met(tasks[num_tasks - 1], true);
  EXPECT_TRUE(newly_ready.empty());

  // All tasks should be finished, check successors are empty
  for (const auto &task : tasks) {
    EXPECT_TRUE(dep.get_successors(task).empty());
  }
}

namespace {

struct Test_task {
  Test_task(std::vector<int> &order, std::mutex &mtx, int id,
            bool delay_this = false)
      : execution_order(order),
        mutex_order(mtx),
        task_id(id),
        delayed(delay_this) {}
  int operator()([[maybe_unused]] unsigned int th_id) {
    if (delayed) {
      std::this_thread::sleep_for(1s);
    }
    std::lock_guard<std::mutex> lock(mutex_order);
    execution_order.push_back(task_id);
    return 0;
  }
  std::vector<int> &execution_order;
  std::mutex &mutex_order;
  int task_id;
  bool delayed{false};
};

template <typename Lwm_clock_type>
void testLwmClockIntegrationExec() {
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Lwm_clock_type>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);
  Task_sequencer gen;

  std::vector<int> execution_order;
  std::mutex mutex_order;

  // Enqueue 5 tasks with delay 0
  for (int i = 1; i <= 5; ++i) {
    Task_id task_id = gen.next_id();
    scheduler.enqueue(schedule_factory.create(task_id, 0),
                      Test_task(execution_order, mutex_order, i));
  }

  // All tasks should execute in parallel since delay=0 <= LWM=0
  scheduler.synchronize();

  // Verify all tasks executed (order may vary due to parallelism)
  ASSERT_EQ(execution_order.size(), 5);
  std::sort(execution_order.begin(), execution_order.end());
  std::vector<int> expected{1, 2, 3, 4, 5};
  EXPECT_EQ(execution_order, expected);

  // LWM should be 5 since all tasks have executed consecutively.
  EXPECT_EQ(clock->now(), 5);
}

}  // namespace

TEST(Scheduler, LwmClockIntegrationReg) {
  testLwmClockIntegrationExec<Clock_lwm_registry>();
}

namespace {

template <typename Lwm_clock_type>
void testLwmClockSequentialExec() {
  // Test Scheduler with LWM clock and stub dependency tracker
  // Tasks have delays task_id-1, execute sequentially, LWM advances to 4
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Lwm_clock_type>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);

  std::vector<int> execution_order;
  std::mutex mutex_order;

  // Enqueue 5 tasks with task_id starting from 1, delay = task_id-1
  for (int i = 0; i < 5; ++i) {
    Task_id task_id(i);
    uint64_t delay = i;  // delay = LWM
    scheduler.enqueue(schedule_factory.create(task_id, delay),
                      Test_task(execution_order, mutex_order, i));
  }

  // Tasks execute sequentially due to time dependencies
  scheduler.synchronize();

  // Verify sequential execution order
  ASSERT_EQ(execution_order.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(execution_order[i], i);
  }

  // LWM should be 5 since all tasks have executed consecutively.
  EXPECT_EQ(clock->now(), 5);
}

}  // namespace

TEST(Scheduler, LwmClockSequentialReg) {
  testLwmClockSequentialExec<Clock_lwm_registry>();
}

namespace {

template <typename Lwm_clock_type>
void testLwmClockDefinedOrderExec() {
  // Test Scheduler with LWM clock and stub dependency tracker
  // Tasks execute at defined order

  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>();
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Lwm_clock_type>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);

  std::vector<int> vec;
  std::mutex mutex_order;

  scheduler.enqueue(schedule_factory.create(Task_id{0}, 0),
                    Test_task(vec, mutex_order, 0));

  scheduler.enqueue(schedule_factory.create(Task_id{1}, 1),
                    Test_task(vec, mutex_order, 1));

  scheduler.enqueue(schedule_factory.create(Task_id{2}, 1),
                    Test_task(vec, mutex_order, 2));

  scheduler.enqueue(schedule_factory.create(Task_id{3}, 1),
                    Test_task(vec, mutex_order, 3, true));

  scheduler.enqueue(schedule_factory.create(Task_id{4}, 3),
                    Test_task(vec, mutex_order, 4));

  scheduler.synchronize();

  EXPECT_EQ(vec[0], 0);

  auto pos_4 = std::distance(vec.begin(), std::find(vec.begin(), vec.end(), 4));
  ASSERT_TRUE((size_t)pos_4 < vec.size());

  auto pos_1 = std::distance(vec.begin(), std::find(vec.begin(), vec.end(), 1));
  ASSERT_TRUE((size_t)pos_1 < vec.size());

  auto pos_2 = std::distance(vec.begin(), std::find(vec.begin(), vec.end(), 2));
  ASSERT_TRUE((size_t)pos_2 < vec.size());

  auto pos_3 = std::distance(vec.begin(), std::find(vec.begin(), vec.end(), 3));
  ASSERT_TRUE((size_t)pos_3 < vec.size());

  ASSERT_TRUE(pos_4 > pos_1);
  ASSERT_TRUE(pos_4 > pos_2);

  // We expect below to be true because we set a long sleep in task_3.
  // We make sure that task_4 is not limited by the execution of task_3, but
  // in reality, task_4 is able to execute in parallel with task_3.
  EXPECT_TRUE(pos_4 < pos_3);

  // LWM should be 5 since all tasks have executed.
  EXPECT_EQ(clock->now(), 5);
}

}  // namespace

TEST(Scheduler, LwmClockDefinedOrderReg) {
  testLwmClockDefinedOrderExec<Clock_lwm_registry>();
}

namespace {

template <typename Lwm_clock_type>
void testLwmClockLoadExec() {
  // Comprehensive load test for LWM clock under concurrent execution
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(4);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Lwm_clock_type>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);

  const int num_tasks = 1000;
  std::vector<int> execution_order;
  std::mutex mutex_order;
  std::atomic<int> tasks_completed{0};

  // Enqueue tasks with increasing delays to create prolonged execution waves
  for (int i = 0; i < num_tasks; ++i) {
    Task_id task_id(i);
    uint64_t delay = i / 10;  // Delays: 0,0,0,...,1,1,1,... up to ~99
    scheduler.enqueue(
        schedule_factory.create(task_id, delay),
        [&execution_order, &mutex_order, &tasks_completed, i](int) {
          std::lock_guard<std::mutex> lock(mutex_order);
          execution_order.push_back(i);
          ++tasks_completed;
          return 0;
        });
  }

  scheduler.synchronize();

  // Verify all tasks executed
  EXPECT_EQ(tasks_completed.load(), num_tasks);
  EXPECT_EQ(execution_order.size(), static_cast<size_t>(num_tasks));

  // LWM should advance through all delay levels (0-99)
  EXPECT_GE(clock->now(), 100);  // Should reach at least delay level 99

  // Ensure no duplicate executions
  std::sort(execution_order.begin(), execution_order.end());
  for (int i = 0; i < num_tasks; ++i) {
    EXPECT_EQ(execution_order[i], i);
  }
}

}  // namespace

TEST(Scheduler, LwmClockLoadTestReg) {
  testLwmClockLoadExec<Clock_lwm_registry>();
}

// Tests for Dependency_adapter_lwm

TEST(DependencyAdapterLwm, LwmDefinedOrder) {
  // Test SEQ_UNINIT (0) commit_parent - should be treated as barrier
  mysql::csa::Dependency_adapter_lwm adapter;
  Task_sequencer gen;

  auto task_id = gen.next_id();
  auto deps = adapter.solve(task_id, 1, 0);  // SEQ_UNINIT commit_parent
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 0);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 2, 1);
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 1);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 3, 1);
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 1);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 4, 1);
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 1);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 5, 3);
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 3);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 6, 0);  // barrier
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 5);

  task_id = gen.next_id();
  deps = adapter.solve(task_id, 7, 3);  // after barrier
  ASSERT_TRUE(deps.first.has_value());
  ASSERT_EQ(deps.first.value(), 5);
}

namespace {

std::atomic_flag block_task;
std::atomic_flag block_error_task;
std::atomic<std::size_t> blocked_tasks_cnt{0};
std::atomic<std::size_t> error_tasks_cnt{0};

struct DummyTask {
  bool is_error() { return m_return_error; }
  int operator()(unsigned int) {
    if (m_return_error) {
      ++error_tasks_cnt;
    } else {
      ++blocked_tasks_cnt;
    }
    block_me.wait(false);
    return 0;
  }
  std::atomic_flag &block_me;
  bool m_return_error{false};
};

}  // namespace

// Synchronize on error, thread pool queue non-empty
//
// This test is checking whether after the applier error, scheduler
// is successfully synchronizing (all tasks are destroyed / executed).
TEST(Scheduler, SchedulerErrorNonEmptyQueue) {
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(4);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);

  block_task.clear();
  block_error_task.clear();
  blocked_tasks_cnt.store(0);
  error_tasks_cnt.store(0);

  // task that will error out
  scheduler.enqueue(schedule_factory.create(Task_id{0}, 0),
                    DummyTask{block_error_task, true});
  // wait
  while (error_tasks_cnt < 1) {
  }

  // fill the queue
  std::size_t do_blocked_tasks_cnt = 10;  // greater than THP workers count
  for (std::size_t id = 0; id < do_blocked_tasks_cnt; ++id) {
    scheduler.enqueue(schedule_factory.create(Task_id{id + 1}, 0),
                      DummyTask{block_task, false});
  }

  // ensure that the rest of the tasks will fill the THP queue
  std::this_thread::sleep_for(std::chrono::microseconds(50));

  // unblock task with error
  block_error_task.test_and_set();
  block_error_task.notify_all();

  // wait for error
  while (!scheduler.is_error()) {
  }

  // synchronization fails
  EXPECT_FALSE(scheduler.synchronize(false));

  // unblock tasks
  block_task.test_and_set();
  block_task.notify_all();
  // synchronization should succeed
  EXPECT_TRUE(scheduler.synchronize());
}

// Synchronize after stop, thread pool queue non-empty
//
// This test is checking whether after the applier stop, scheduler
// is successfully synchronizing (all tasks are destroyed / executed).
TEST(Scheduler, SchedulerStopNonEmptyQueue) {
  std::ignore = Statistics_map::init_statistics(0);
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(4);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, clock, std::move(dep));
  Schedule_factory schedule_factory(clock);

  block_task.clear();
  block_error_task.clear();
  blocked_tasks_cnt.store(0);
  error_tasks_cnt.store(0);

  // fill the queue
  std::size_t do_blocked_tasks_cnt = 10;  // greater than THP workers count
  for (std::size_t id = 0; id < do_blocked_tasks_cnt; ++id) {
    scheduler.enqueue(schedule_factory.create(Task_id{id + 1}, 0),
                      DummyTask{block_task, false});
  }

  // ensure that the rest of the tasks will fill the THP queue, verified by
  // printouts that 50 is enough
  std::this_thread::sleep_for(std::chrono::microseconds(50));

  // stop the scheduler
  scheduler.stop_now();

  // wait a little bit
  std::this_thread::sleep_for(std::chrono::microseconds(50));

  // synchronization fails
  EXPECT_FALSE(scheduler.synchronize(false));

  // unblock tasks
  block_task.test_and_set();
  block_task.notify_all();
  // synchronization should succeed
  EXPECT_TRUE(scheduler.synchronize());
}

}  // namespace mysql::scheduler
