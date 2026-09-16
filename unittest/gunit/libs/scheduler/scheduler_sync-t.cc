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

using namespace std;

namespace mysql::scheduler {
namespace {

static constexpr bool quiet = true;

std::random_device dev;
std::mt19937 rng(dev());
std::uniform_int_distribution<std::mt19937::result_type> dist(0, 10);
using Vec_time = std::vector<std::size_t>;

/// Task executed in the scheduler
/// @param sleep_duration_ms Simulates a task workload, this is the number of
/// milliseconds the task will actively wait during its execution
template <std::size_t sleep_duration_ms>
struct Task_sleepy {
  Task_sleepy(Vec_time &check_time_vec, Scheduler_clock_ptr shared_clock,
              std::mutex &mutex_ref)
      : m_check_time_vec(check_time_vec),
        m_shared_clock(shared_clock),
        m_mutex(mutex_ref) {}
  void operator()(unsigned int) {
    auto start = std::chrono::system_clock::now();
    std::chrono::milliseconds elapsed;
    do {
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - start);
    } while (static_cast<std::size_t>(elapsed.count()) < sleep_duration_ms);
    {
      std::scoped_lock lock(m_mutex);
      m_check_time_vec.push_back(m_shared_clock->now());
    }
    ++m_check;
  }
  /// Used to check if all tasks executed
  std::atomic<std::size_t> m_check{0};
  Vec_time &m_check_time_vec;
  Scheduler_clock_ptr m_shared_clock;
  std::mutex &m_mutex;
};

/// Task executed in the scheduler, various length
struct Task_random_wait {
  Task_random_wait(Vec_time &check_time_vec, Scheduler_clock_ptr shared_clock,
                   std::mutex &mutex_ref)
      : m_check_time_vec(check_time_vec),
        m_shared_clock(shared_clock),
        m_mutex(mutex_ref) {}
  void operator()(unsigned int) {
    std::this_thread::sleep_for(
        std::chrono::duration<int, std::micro>{dist(rng)});
    {
      std::scoped_lock lock(m_mutex);
      m_check_time_vec.push_back(m_shared_clock->now());
    }
    ++m_check;
  }
  /// Used to check if all tasks executed
  std::atomic<std::size_t> m_check{0};
  Vec_time &m_check_time_vec;
  Scheduler_clock_ptr m_shared_clock;
  std::mutex &m_mutex;
};

/// @brief Tests theoretical performance of the scheduler
/// @param sleep_duration_ms task duration in ms
/// @param sequential_exec When true, executes tasks sequentially, but
///   sequential execution of the task is enforced by the logical clock
///   dependencies, not the number of threads in the thread pool. When false,
///   tasks execute in parallel by threads contained in the thread pool
/// @param worker_pool_size Number of threads in the thread pool
template <class Task_type, bool sequential_exec>
void sync_test(
    std::size_t iterations,
    std::size_t worker_pool_size = std::thread::hardware_concurrency()) {
  Vec_time check_time;
  Vec_time check_time_sorted;
  std::mutex check_time_mutex;

  std::ignore = Statistics_map::init_statistics(0);
  Scheduler_clock_ptr local_clock = std::make_shared<Clock_lwm_registry>();
  std::shared_ptr<Thread_pool<Task_result>> th_pool =
      std::make_shared<Thread_pool<Task_result>>(worker_pool_size);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Dependency_tracker_ptr dep(new Dependency_tracker_stub());
  Scheduler scheduler(th_pool, local_clock, std::move(dep));
  Task_type task1(std::ref(check_time), local_clock,
                  std::ref(check_time_mutex));
  Schedule_factory schedule_factory(local_clock);
  Task_sequencer gen;

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
  scheduler.synchronize(true);
  if constexpr (!quiet) std::cout << "Synchronized all tasks" << std::endl;
  ASSERT_EQ(task1.m_check.load(), iterations);
  ASSERT_EQ(check_time.size(), iterations);
  check_time_sorted = check_time;
  std::sort(check_time_sorted.begin(), check_time_sorted.end());
  auto check_time_it = check_time.begin();
  auto check_time_sorted_it = check_time_sorted.begin();
  std::size_t current_time_captured = 0;
  while (check_time_it != check_time.end()) {
    if (sequential_exec) {
      ASSERT_EQ(current_time_captured++, *(check_time_it));
    } else {
      ASSERT_LE(*(check_time_it), iterations);
    }
    ASSERT_EQ(*(check_time_it++), *(check_time_sorted_it++));
  }
}

}  // namespace

// sequential execution, forced by logical clock, 0ms task
TEST(SchedulerSync, SeqShort) { sync_test<Task_sleepy<0>, true>(1000, 64); }

// parellel execution, 1 thread, 0ms task
TEST(SchedulerSync, ParallelShort1) {
  sync_test<Task_sleepy<0>, false>(1000, 1);
}

// parellel execution, 4 threads, 0ms task
TEST(SchedulerSync, ParallelShort4) {
  sync_test<Task_sleepy<0>, false>(1000, 4);
}

// parellel execution, 8 threads, 0ms task
TEST(SchedulerSync, ParallelShort8) {
  sync_test<Task_sleepy<0>, false>(1000, 8);
}

// parellel execution, 16 threads, 0ms task
TEST(SchedulerSync, ParallelShort16) {
  sync_test<Task_sleepy<0>, false>(1000, 16);
}

// parellel execution, 32 threads, 0ms task
TEST(SchedulerSync, ParallelShort32) {
  sync_test<Task_sleepy<0>, false>(1000, 32);
}

// parellel execution, 64 threads, 0ms task
TEST(SchedulerSync, ParallelShort64) {
  sync_test<Task_sleepy<0>, false>(1000, 64);
}

// sequential execution, 8 threads, 0ms task
TEST(SchedulerSync, SequentialSyncLogClock) {
  sync_test<Task_random_wait, true>(10000, 8);
}

}  // namespace mysql::scheduler
