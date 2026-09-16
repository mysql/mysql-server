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

using namespace std;

namespace mysql::scheduler {
namespace {

static constexpr bool quiet = true;

/// Task executed in the scheduler
/// @param sleep_duration_ms Simulates a task workload, this is the number of
/// milliseconds the task will actively wait during its execution
template <std::size_t sleep_duration_ms>
struct Task_sleepy {
  Task_sleepy() {}
  void operator()(unsigned int) {
    auto start = std::chrono::system_clock::now();
    std::chrono::milliseconds elapsed;
    do {
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - start);
    } while (static_cast<std::size_t>(elapsed.count()) < sleep_duration_ms);
    ++m_check;
  }
  /// Used to check if all tasks executed
  std::atomic<std::size_t> m_check{0};
};

/// @brief Tests theoretical performance of the scheduler
/// @param sleep_duration_ms task duration in ms
/// @param worker_pool_size Number of threads in the thread pool
template <std::size_t sleep_duration_ms>
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
  auto prev_id = gen.next_id();
  scheduler.enqueue(schedule_factory.create(prev_id, 0), std::ref(task1));

  for (std::size_t it = 1; it < iterations; ++it) {
    scheduler.enqueue_after(prev_id, schedule_factory.create(gen.next_id(), 0),
                            std::ref(task1));
  }
  if constexpr (!quiet)
    std::cout << "Done enqueueing task, waiting for sync" << std::endl;
  scheduler.synchronize();
  if constexpr (!quiet) std::cout << "Synchronized all tasks" << std::endl;
  ASSERT_EQ(task1.m_check.load(), iterations);
}

}  // namespace

// parellel execution, 1 thread, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep1) {
  perf_test_dependency_tracking<0>(1000, 1);
}

// parellel execution, 4 threads, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep4) {
  perf_test_dependency_tracking<0>(1000, 4);
}

// parellel execution, 8 threads, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep8) {
  perf_test_dependency_tracking<0>(1000, 8);
}

// parellel execution, 16 threads, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep16) {
  perf_test_dependency_tracking<0>(1000, 16);
}

// parellel execution, 32 threads, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep32) {
  perf_test_dependency_tracking<0>(1000, 32);
}

// parellel execution, 64 threads, 0ms task
TEST(SchedulerPerfDependencies, ParallelShortDep64) {
  perf_test_dependency_tracking<0>(1000, 64);
}

}  // namespace mysql::scheduler
