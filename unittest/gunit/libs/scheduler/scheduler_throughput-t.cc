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
static constexpr std::size_t max_threads = 512;
static constexpr std::size_t num_tasks_base = 200000;
static constexpr std::size_t num_us = 10;

/// Task executed in the scheduler
/// @param sleep_duration_us Simulates a task workload, this is the number of
/// microseconds the task will actively wait during its execution
template <std::size_t sleep_duration_us>
struct Task_sleepy {
  Task_sleepy() { m_check.init(max_threads); }
  void operator()([[maybe_unused]] unsigned int thread_id) {
    if constexpr (sleep_duration_us > 0) {
      auto start = std::chrono::system_clock::now();
      std::chrono::microseconds elapsed;
      do {
        elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - start);
      } while (static_cast<std::size_t>(elapsed.count()) < sleep_duration_us);
    }
    m_check.add(1, thread_id);
  }
  Sharded_counter m_check;
};

/// @brief Tests theoretical performance of the scheduler
/// @param sleep_duration_us task duration in ms
/// @param sequential_exec When true, executes tasks sequentially, but
///   sequential execution of the task is enforced by the logical clock
///   dependencies, not the number of threads in the thread pool. When false,
///   tasks execute in parallel by threads contained in the thread pool
/// @param worker_pool_size Number of threads in the thread pool
template <std::size_t sleep_duration_us, bool sequential_exec,
          typename Scheduler_type = Scheduler>
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
  Scheduler_type scheduler(th_pool, local_clock, std::move(dep));
  Task_sleepy<sleep_duration_us> task1;
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
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - start);
  std::cout << "Tasks per second: " << iterations * 1.0 / elapsed.count() * 1e3
            << "kps" << std::endl;
  ASSERT_EQ(task1.m_check.get(), iterations);
}

}  // namespace

// parellel execution, LC
TEST(SchedulerPerf, sched1) {
  perf_test<num_us, false>(num_tasks_base / 32, 1);
}

// parellel execution, LC
TEST(SchedulerPerf, sched2) {
  perf_test<num_us, false>(num_tasks_base / 16, 2);
}

// parellel execution, LC
TEST(SchedulerPerf, sched4) { perf_test<num_us, false>(num_tasks_base / 8, 4); }

// parellel execution, LC
TEST(SchedulerPerf, sched8) { perf_test<num_us, false>(num_tasks_base / 4, 8); }

// parellel execution, LC
TEST(SchedulerPerf, sched16) {
  perf_test<num_us, false>(num_tasks_base / 2, 16);
}

// parellel execution, LC
TEST(SchedulerPerf, sched32) { perf_test<num_us, false>(num_tasks_base, 32); }

// parellel execution, LC
TEST(SchedulerPerf, sched64) { perf_test<num_us, false>(num_tasks_base, 64); }

// parellel execution, LC
TEST(SchedulerPerf, sched128) { perf_test<num_us, false>(num_tasks_base, 128); }

}  // namespace mysql::scheduler
