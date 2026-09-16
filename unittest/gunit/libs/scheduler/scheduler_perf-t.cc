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

#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/dependency_tracker_single_predecessor_example.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_sequencer.h"

#include "mysql/scheduler/sharded_counter.h"

using namespace std;
using namespace mysql::concurrency;

namespace mysql::scheduler {
namespace {

static constexpr bool quiet = true;
static constexpr std::size_t max_threads = 1024;

/// Task executed in the scheduler
/// @param sleep_duration_ms Simulates a task workload, this is the number of
/// milliseconds the task will actively wait during its execution
template <std::size_t sleep_duration_ms>
struct Task_sleepy {
  Task_sleepy() { m_check.init(max_threads); }
  void operator()(unsigned int thread_id) {
    if constexpr (sleep_duration_ms > 0) {
      auto start = std::chrono::system_clock::now();
      std::chrono::milliseconds elapsed;
      do {
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start);
      } while (static_cast<std::size_t>(elapsed.count()) < sleep_duration_ms);
    }
    m_check.add(1, thread_id);
  }
  /// Used to check if all tasks executed
  Sharded_counter m_check;
};

/// @brief Tests theoretical performance of the scheduler
/// @param sleep_duration_ms task duration in ms
/// @param sequential_exec When true, executes tasks sequentially, but
///   sequential execution of the task is enforced by the logical clock
///   dependencies, not the number of threads in the thread pool. When false,
///   tasks execute in parallel by threads contained in the thread pool
/// @param worker_pool_size Number of threads in the thread pool
template <std::size_t sleep_duration_ms, bool sequential_exec>
void perf_test(
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
  Task_sleepy<sleep_duration_ms> task1;
  Schedule_factory schedule_factory(local_clock);
  Task_sequencer gen;

  auto start = std::chrono::system_clock::now();

  for (std::size_t it = 0; it < iterations; ++it) {
    if constexpr (sequential_exec) {
      std::ignore = scheduler.enqueue(
          schedule_factory.create(gen.next_id(), it), std::ref(task1));
    } else {
      std::ignore = scheduler.enqueue(schedule_factory.create(gen.next_id(), 0),
                                      std::ref(task1));
    }
  }
  if constexpr (!quiet)
    std::cout << "Done enqueueing task, waiting for sync" << std::endl;
  scheduler.synchronize();
  if constexpr (!quiet) std::cout << "Synchronized all tasks" << std::endl;
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start);
  std::cout << "Tasks per second: " << iterations * 1.0 / elapsed.count()
            << "kps" << std::endl;
  ASSERT_EQ(task1.m_check.get(), iterations);
}

/// @brief Tests theoretical performance of the scheduler with dependency
/// tracking
/// @param sleep_duration_ms task duration in ms
/// @param sequential_exec When true, creates sequential chain
/// (task1->task2->task3).
///   When false, creates fan-out dependencies (all depend on first task, then
///   parallel).
/// @param worker_pool_size Number of threads in the thread pool
template <std::size_t sleep_duration_ms, bool sequential_exec>
void perf_test_dependency_tracking(
    std::size_t iterations,
    std::size_t worker_pool_size = std::thread::hardware_concurrency()) {
  std::ignore = Statistics_map::init_statistics(0);
  Scheduler_clock_ptr local_clock = std::make_shared<Clock_lwm_registry>();
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(worker_pool_size);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Dependency_tracker_ptr dep(new Dependency_tracker_single_predecessor());
  Scheduler scheduler(th_pool, local_clock, std::move(dep));
  Task_sleepy<sleep_duration_ms> task1;
  Schedule_factory schedule_factory(local_clock);
  Task_sequencer gen;

  auto start = std::chrono::system_clock::now();

  if constexpr (sequential_exec) {
    // Create sequential chain: task1 -> task2 -> task3 -> ...
    auto prev_id = gen.next_id();
    scheduler.enqueue(schedule_factory.create(prev_id, 0), std::ref(task1));
    for (std::size_t it = 1; it < iterations; ++it) {
      auto next_id = gen.next_id();
      scheduler.enqueue_after(prev_id, schedule_factory.create(next_id, 0),
                              std::ref(task1));
      prev_id = next_id;
    }
  } else {
    // Create fan-out dependencies: all tasks depend on the first task
    // task1 -> task2, task1 -> task3, task1 -> task4, ... (parallel after
    // task1)
    auto first_task_id = gen.next_id();
    scheduler.enqueue(schedule_factory.create(first_task_id, 0),
                      std::ref(task1));
    for (std::size_t it = 1; it < iterations; ++it) {
      scheduler.enqueue_after(first_task_id,
                              schedule_factory.create(gen.next_id(), 0),
                              std::ref(task1));
    }
  }

  if constexpr (!quiet)
    std::cout << "Done enqueueing task, waiting for sync" << std::endl;
  scheduler.synchronize();
  if constexpr (!quiet) std::cout << "Synchronized all tasks" << std::endl;
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start);
  std::cout << "Tasks per second: " << iterations * 1.0 / elapsed.count()
            << "kps" << std::endl;
  ASSERT_EQ(task1.m_check.get(), iterations);
}

// // parellel execution, load test, 64 threads, used to check CPU activity
// TEST(SchedulerPerf, ParallelLoad) { perf_test<10, false>(100000, 64); }

}  // namespace

// sequential execution, forced by logical clock, 0ms task
TEST(SchedulerPerf, SeqShort) { perf_test<0, true>(1000, 64); }

// parellel execution, 1 thread, 0ms task
TEST(SchedulerPerf, ParallelShort1) { perf_test<0, false>(1000, 1); }

// parellel execution, 4 threads, 0ms task
TEST(SchedulerPerf, ParallelShort4) { perf_test<0, false>(1000, 4); }

// parellel execution, 8 threads, 0ms task
TEST(SchedulerPerf, ParallelShort8) { perf_test<0, false>(1000, 8); }

// parellel execution, 16 threads, 0ms task
TEST(SchedulerPerf, ParallelShort16) { perf_test<0, false>(1000, 16); }

// parellel execution, 32 threads, 0ms task
TEST(SchedulerPerf, ParallelShort32) { perf_test<0, false>(1000, 32); }

// parellel execution, 64 threads, 0ms task
TEST(SchedulerPerf, ParallelShort64) { perf_test<0, false>(1000, 64); }

// sequential execution forced by logical clock, 10ms task
TEST(SchedulerPerf, SeqLong) { perf_test<10, true>(1000, 64); }

// parellel execution, 1 thread, 10ms task
TEST(SchedulerPerf, ParallelLong1) { perf_test<10, false>(1000, 1); }

// parellel execution, 4 threads, 10ms task
TEST(SchedulerPerf, ParallelLong4) { perf_test<10, false>(1000, 4); }

// parellel execution, 8 threads, 10ms task
TEST(SchedulerPerf, ParallelLong8) { perf_test<10, false>(1000, 8); }

// parellel execution, 16 threads, 10ms task
TEST(SchedulerPerf, ParallelLong16) { perf_test<10, false>(1000, 16); }

// parellel execution, 32 threads, 10ms task
TEST(SchedulerPerf, ParallelLong32) { perf_test<10, false>(1000, 32); }

// parellel execution, 64 threads, 10ms task
TEST(SchedulerPerf, ParallelLong64) { perf_test<10, false>(1000, 64); }

// sequential execution with chain dependencies, 1 thread, 0ms task
TEST(SchedulerPerf, SeqShortDep1) {
  perf_test_dependency_tracking<0, true>(1000, 1);
}

// sequential execution with chain dependencies, 4 threads, 0ms task
TEST(SchedulerPerf, SeqShortDep4) {
  perf_test_dependency_tracking<0, true>(1000, 4);
}

// sequential execution with chain dependencies, 8 threads, 0ms task
TEST(SchedulerPerf, SeqShortDep8) {
  perf_test_dependency_tracking<0, true>(1000, 8);
}

// sequential execution with chain dependencies, 16 threads, 0ms task
TEST(SchedulerPerf, SeqShortDep16) {
  perf_test_dependency_tracking<0, true>(1000, 16);
}

// sequential execution with chain dependencies, 32 threads, 0ms task
TEST(SchedulerPerf, SeqShortDep32) {
  perf_test_dependency_tracking<0, true>(1000, 32);
}

// sequential execution with chain dependencies, 64 threads, 0ms task
TEST(SchedulerPerf, SeqShortDep64) {
  perf_test_dependency_tracking<0, true>(1000, 64);
}

// parallel execution with fan-out dependencies, 1 thread, 0ms task
TEST(SchedulerPerf, ParallelShortDep1) {
  perf_test_dependency_tracking<0, false>(1000, 1);
}

// parallel execution with fan-out dependencies, 4 threads, 0ms task
TEST(SchedulerPerf, ParallelShortDep4) {
  perf_test_dependency_tracking<0, false>(1000, 4);
}

// parallel execution with fan-out dependencies, 8 threads, 0ms task
TEST(SchedulerPerf, ParallelShortDep8) {
  perf_test_dependency_tracking<0, false>(1000, 8);
}

// parallel execution with fan-out dependencies, 16 threads, 0ms task
TEST(SchedulerPerf, ParallelShortDep16) {
  perf_test_dependency_tracking<0, false>(1000, 16);
}

// parallel execution with fan-out dependencies, 32 threads, 0ms task
TEST(SchedulerPerf, ParallelShortDep32) {
  perf_test_dependency_tracking<0, false>(1000, 32);
}

// parallel execution with fan-out dependencies, 64 threads, 0ms task
TEST(SchedulerPerf, ParallelShortDep64) {
  perf_test_dependency_tracking<0, false>(1000, 64);
}

// LWM clock performance tests

namespace {

/// @brief Tests performance of the scheduler with LWM clock
template <typename Lwm_clock_type, std::size_t sleep_duration_ms>
void perf_test_lwm(
    std::size_t iterations,
    std::size_t worker_pool_size = std::thread::hardware_concurrency()) {
  std::ignore = Statistics_map::init_statistics(0);
  Scheduler_clock_ptr local_clock = std::make_shared<Lwm_clock_type>();
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(worker_pool_size);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, local_clock, std::move(dep));
  Task_sleepy<sleep_duration_ms> task1;
  Schedule_factory schedule_factory(local_clock);
  Task_sequencer gen;

  auto start = std::chrono::system_clock::now();

  for (std::size_t it = 0; it < iterations; ++it) {
    std::ignore = scheduler.enqueue(schedule_factory.create(gen.next_id(), 0),
                                    std::ref(task1));
  }
  if constexpr (!quiet)
    std::cout << "Done enqueueing task, waiting for sync" << std::endl;
  scheduler.synchronize();
  if constexpr (!quiet) std::cout << "Synchronized all tasks" << std::endl;
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start);
  std::cout << "Tasks per second: " << iterations * 1.0 / elapsed.count()
            << "kps" << std::endl;
  ASSERT_EQ(task1.m_check.get(), iterations);
}

}  // namespace

// LWM registry clock, 1 thread, 0ms task
TEST(SchedulerPerf, LwmRegShort1) {
  perf_test_lwm<Clock_lwm_registry, 0>(1000, 1);
}

// LWM registry clock, 4 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort4) {
  perf_test_lwm<Clock_lwm_registry, 0>(1000, 4);
}

// LWM registry clock, 8 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort8) {
  perf_test_lwm<Clock_lwm_registry, 0>(10000, 8);
}

// LWM registry clock, 16 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort16) {
  perf_test_lwm<Clock_lwm_registry, 0>(10000, 16);
}

// LWM registry clock, 32 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort32) {
  perf_test_lwm<Clock_lwm_registry, 0>(10000, 32);
}

// LWM registry clock, 64 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort64) {
  perf_test_lwm<Clock_lwm_registry, 0>(10000, 64);
}

// LWM registry clock, 1 thread, 10ms task
TEST(SchedulerPerf, LwmRegLong1) {
  perf_test_lwm<Clock_lwm_registry, 10>(1000, 1);
}

// LWM registry clock, 4 threads, 10ms task
TEST(SchedulerPerf, LwmRegLong4) {
  perf_test_lwm<Clock_lwm_registry, 10>(1000, 4);
}

// LWM registry clock, 8 threads, 10ms task
TEST(SchedulerPerf, LwmRegLong8) {
  perf_test_lwm<Clock_lwm_registry, 10>(10000, 8);
}

// LWM registry clock, 16 threads, 10ms task
TEST(SchedulerPerf, LwmRegLong16) {
  perf_test_lwm<Clock_lwm_registry, 10>(10000, 16);
}

// LWM registry clock, 32 threads, 10ms task
TEST(SchedulerPerf, LwmRegLong32) {
  perf_test_lwm<Clock_lwm_registry, 10>(10000, 32);
}

// LWM registry clock, 64 threads, 10ms task
TEST(SchedulerPerf, LwmRegLong64) {
  perf_test_lwm<Clock_lwm_registry, 10>(1000, 64);
}

// LWM registry clock, 128 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort128) {
  perf_test_lwm<Clock_lwm_registry, 0>(10000, 128);
}

// LWM registry clock, 256 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort256) {
  perf_test_lwm<Clock_lwm_registry, 0>(25000, 256);
}

// LWM registry clock, 512 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort512) {
  perf_test_lwm<Clock_lwm_registry, 0>(50000, 512);
}

// LWM registry clock, 1024 threads, 0ms task
TEST(SchedulerPerf, LwmRegShort1024) {
  perf_test_lwm<Clock_lwm_registry, 0>(100000, 1024);
}

}  // namespace mysql::scheduler
