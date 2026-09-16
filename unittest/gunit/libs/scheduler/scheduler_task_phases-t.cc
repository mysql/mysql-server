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
#include <array>
#include <cstring>
#include <future>
#include <numeric>
#include <random>
#include <sstream>

#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/commit_order_clock.h"
#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/task_sequencer.h"

using namespace std;

namespace mysql::scheduler {
namespace {

static constexpr bool quiet = true;

std::random_device dev;
std::mt19937 rng(dev());
std::uniform_int_distribution<std::mt19937::result_type> dist(0, 10);
std::uniform_int_distribution<std::mt19937::result_type> dist_2(0, 100000);
using Vec_type = std::vector<int>;

/// Task executed in the scheduler, various length
struct Task_random_wait {
  Task_random_wait(Vec_type &task_prepare_sequence,
                   Vec_type &task_commit_sequence, Vec_type &task_commit_finish,
                   int task_sequence_number,
                   std::atomic<std::size_t> &prepare_cnt,
                   std::atomic<std::size_t> &commit_cnt,
                   std::atomic<std::size_t> &commit_finish_cnt)
      : m_task_prepare_sequence(task_prepare_sequence),
        m_task_commit_sequence(task_commit_sequence),
        m_task_commit_finish(task_commit_finish),
        m_task_sequence_number(task_sequence_number),
        m_prepare_cnt(prepare_cnt),
        m_commit_cnt(commit_cnt),
        m_commit_finish(commit_finish_cnt) {
    // assert that thread may access its slot in output vectors
    assert(static_cast<int>(m_task_prepare_sequence.size()) >
           m_task_sequence_number);
    assert(static_cast<int>(task_commit_sequence.size()) >
           m_task_sequence_number);
    assert(static_cast<int>(task_commit_finish.size()) >
           m_task_sequence_number);
  }
  void operator()([[maybe_unused]] unsigned int thread_id) {
    if (m_phase == 0) {
      // prepare
      auto order = m_prepare_cnt.fetch_add(1);
      m_task_prepare_sequence[m_task_sequence_number] = order;
    } else if (m_phase == 1) {  // sequential commit
      auto order = m_commit_cnt.fetch_add(1);
      m_task_commit_sequence[m_task_sequence_number] = order;
    } else if (m_phase == 2) {  // out of order commit finish phase
      auto order = m_commit_finish.fetch_add(1);
      m_task_commit_finish[m_task_sequence_number] = order;
    } else {  // should not be called
      assert(false);
    }
    ++m_phase;
  }

  Vec_type &m_task_prepare_sequence;
  Vec_type &m_task_commit_sequence;
  Vec_type &m_task_commit_finish;
  int m_task_sequence_number{0};
  std::atomic<std::size_t> &m_prepare_cnt;
  std::atomic<std::size_t> &m_commit_cnt;
  std::atomic<std::size_t> &m_commit_finish;
  int m_phase{0};
};

bool check_sequence(const Vec_type &vec) {
  Vec_type correct_sequence(vec.size());
  std::iota(correct_sequence.begin(), correct_sequence.end(), 0);
  auto vec_it = vec.begin();
  auto correct_sequence_it = correct_sequence.begin();
  while (vec_it != vec.end()) {
    if (*vec_it++ != *correct_sequence_it++) {
      return false;
    }
  }
  return true;
}

bool sort_and_check_sequence(const Vec_type &vec) {
  Vec_type sorted(vec);
  std::sort(sorted.begin(), sorted.end());
  return check_sequence(sorted);
}

/// @brief Tests commit schedule dependencies
void test_commit_schedule(
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

  std::atomic<std::size_t> prepare_cnt{0};
  std::atomic<std::size_t> commit_cnt{0};
  std::atomic<std::size_t> commit_finish{0};

  Scheduler_clock_ptr commit_clock = std::make_shared<Commit_order_clock>();

  Vec_type prepare_sequence;
  Vec_type commit_order_sequence;
  Vec_type commit_finish_sequence;
  for (std::size_t it = 0; it < iterations; ++it) {
    prepare_sequence.push_back(-1);
    commit_order_sequence.push_back(-1);
    commit_finish_sequence.push_back(-1);
  }
  Schedule_factory schedule_factory(local_clock, commit_clock);
  Task_sequencer gen;
  scheduler.register_phase(commit_clock);
  bool is_trx = true;

  for (std::size_t it = 0; it < iterations; ++it) {
    Task_random_wait current_task(
        std::ref(prepare_sequence), std::ref(commit_order_sequence),
        std::ref(commit_finish_sequence), it, std::ref(prepare_cnt),
        std::ref(commit_cnt), std::ref(commit_finish));
    std::ignore = scheduler.enqueue(
        schedule_factory.create(gen.next_id(), 0, is_trx), current_task);
  }
  scheduler.synchronize(true);
  bool prep_equal = check_sequence(prepare_sequence);
  bool co_equal = check_sequence(commit_order_sequence);
  bool fi_equal = check_sequence(commit_finish_sequence);
  bool prep_sorted_equal = sort_and_check_sequence(prepare_sequence);
  bool fi_sorted_equal = sort_and_check_sequence(commit_finish_sequence);
  ASSERT_TRUE(prep_sorted_equal);  // prepare may be out of order
  ASSERT_TRUE(co_equal);           // commit must be in order
  ASSERT_TRUE(fi_sorted_equal);    // commit finish may be out of order
  if (prep_equal == true) {
    // unlikely, but possible
    std::cout
        << "Warning: prepare executed in order, that is unlikely to happen"
        << std::endl;
  }
  if (fi_equal == true) {
    // unlikely, but possible
    std::cout
        << "Warning: after satisfying start-to-start commit order dependency, "
           "commit finished in order - that is unlikely to happen"
        << std::endl;
  }
}

}  // namespace

TEST(SchedulerPhases, Test64) { test_commit_schedule(1000, 64); }

namespace {

// Stop flag used in the "stop test". It will stop enqueueing thread
// and task execution
std::atomic<bool> m_stop_it{false};

/// Task executed in the "stop" test, various length
struct Task_two_phase {
  Task_two_phase() {}
  void operator()([[maybe_unused]] unsigned int thread_id) {
    if (m_phase == 0) {
      if (is_stopped()) {
        m_error = true;
        return;
      }
      std::this_thread::sleep_for(
          std::chrono::duration<int, std::micro>{dist(rng)});
    } else if (m_phase == 1 || m_phase == 2) {
      if (is_stopped()) {
        m_error = true;
        return;
      }
      std::this_thread::sleep_for(
          std::chrono::duration<int, std::micro>{dist(rng)});
      if (is_stopped()) {
        return;
      }
    } else {  // should not be called
      assert(false);
    }
    ++m_phase;
  }
  bool is_stopped() { return m_stop_it.load() == true; }
  bool is_error() { return m_error == true; }
  int m_phase{0};
  bool m_error{false};
};

void stop_with_concurrent_enqueue() {
  m_stop_it = false;
  std::ignore = Statistics_map::init_statistics(0);
  auto th_pool = std::make_shared<Thread_pool<Task_result>>(4);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Scheduler_clock_ptr commit_clock = std::make_shared<Commit_order_clock>();
  Dependency_tracker_ptr dep = std::make_unique<Dependency_tracker_stub>();
  Scheduler scheduler(th_pool, clock, std::move(dep));
  scheduler.register_phase(commit_clock);
  Schedule_factory schedule_factory(clock, commit_clock);

  std::size_t tasks_num = 200000;

  std::thread enqueuer([&scheduler, &schedule_factory, &tasks_num]() {
    Task_sequencer gen;
    std::size_t enq_id = 0;
    bool success = true;
    while (success && enq_id < tasks_num) {
      auto task_id = gen.next_id();
      Task_two_phase waiting_task;
      std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
      success = scheduler.enqueue(schedule_factory.create(task_id, 0, true),
                                  waiting_task);
      ++enq_id;
      std::this_thread::yield();
    }
  });

  std::thread stopper([&scheduler]() {
    std::this_thread::sleep_for(std::chrono::microseconds(dist_2(rng)));
    scheduler.stop_now();
    m_stop_it = true;
  });

  enqueuer.join();
  stopper.join();

  EXPECT_TRUE(scheduler.synchronize(true, true));
  scheduler.deinit();
  th_pool->end_execution();
  th_pool.reset();
}

}  // namespace

TEST(SchedulerPhases, StopWithConcurrentEnqueue) {
  std::size_t test_num = 100;
  std::size_t current_test_cnt = test_num;
  std::size_t progress_cnt = 0;
  while (current_test_cnt > 0) {
    stop_with_concurrent_enqueue();
    --current_test_cnt;
    if (current_test_cnt % 100 == 0 && !quiet) {
      ++progress_cnt;
      std::cout << "iteration: " << progress_cnt
                << " out of: " << test_num / 100 << " finished." << std::endl;
    }
  }
}

namespace {

struct Task_stop_while_phase_zero_running {
  Task_stop_while_phase_zero_running(std::atomic<bool> &phase_zero_entered,
                                     std::atomic<bool> &release_phase_zero,
                                     std::atomic<bool> &stop_requested,
                                     std::atomic<bool> &phase_zero_finished,
                                     std::atomic<bool> &phase_one_executed)
      : m_phase_zero_entered(phase_zero_entered),
        m_release_phase_zero(release_phase_zero),
        m_stop_requested(stop_requested),
        m_phase_zero_finished(phase_zero_finished),
        m_phase_one_executed(phase_one_executed) {}

  void operator()([[maybe_unused]] unsigned int thread_id) {
    if (m_phase == 0) {
      m_phase_zero_entered.store(true);
      m_phase_zero_entered.notify_one();
      m_release_phase_zero.wait(false);
      if (m_stop_requested.load()) {
        m_error = true;
      }
      m_phase_zero_finished.store(true);
      m_phase_zero_finished.notify_one();
    } else if (m_phase == 1) {
      m_phase_one_executed.store(true);
    } else {
      assert(false);
    }
    ++m_phase;
  }

  bool is_error() { return m_error; }

  std::atomic<bool> &m_phase_zero_entered;
  std::atomic<bool> &m_release_phase_zero;
  std::atomic<bool> &m_stop_requested;
  std::atomic<bool> &m_phase_zero_finished;
  std::atomic<bool> &m_phase_one_executed;
  int m_phase{0};
  bool m_error{false};
};

template <std::size_t task_count>
struct Task_stop_while_phase_zero_running_many {
  Task_stop_while_phase_zero_running_many(
      std::size_t task_index,
      std::array<std::atomic<bool>, task_count> &phase_zero_entered,
      std::array<std::atomic<bool>, task_count> &release_phase_zero,
      std::atomic<bool> &stop_requested,
      std::array<std::atomic<bool>, task_count> &phase_zero_finished,
      std::array<std::atomic<bool>, task_count> &phase_one_executed)
      : m_task_index(task_index),
        m_phase_zero_entered(phase_zero_entered),
        m_release_phase_zero(release_phase_zero),
        m_stop_requested(stop_requested),
        m_phase_zero_finished(phase_zero_finished),
        m_phase_one_executed(phase_one_executed) {}

  void operator()([[maybe_unused]] unsigned int thread_id) {
    if (m_phase == 0) {
      m_phase_zero_entered[m_task_index].store(true);
      m_phase_zero_entered[m_task_index].notify_one();
      m_release_phase_zero[m_task_index].wait(false);
      if (m_stop_requested.load()) {
        m_error = true;
      }
      m_phase_zero_finished[m_task_index].store(true);
      m_phase_zero_finished[m_task_index].notify_one();
    } else if (m_phase == 1) {
      m_phase_one_executed[m_task_index].store(true);
    } else {
      assert(false);
    }
    ++m_phase;
  }

  bool is_error() { return m_error; }

  std::size_t m_task_index{0};
  std::array<std::atomic<bool>, task_count> &m_phase_zero_entered;
  std::array<std::atomic<bool>, task_count> &m_release_phase_zero;
  std::atomic<bool> &m_stop_requested;
  std::array<std::atomic<bool>, task_count> &m_phase_zero_finished;
  std::array<std::atomic<bool>, task_count> &m_phase_one_executed;
  int m_phase{0};
  bool m_error{false};
};

}  // namespace

TEST(SchedulerPhases, StopNowWaitsForRunningPhaseZeroTask) {
  std::ignore = Statistics_map::init_statistics(0);
  auto th_pool = std::make_shared<Thread_pool<Task_result>>(1);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Scheduler_clock_ptr commit_clock = std::make_shared<Commit_order_clock>();
  Dependency_tracker_ptr dep = std::make_unique<Dependency_tracker_stub>();
  Scheduler scheduler(th_pool, clock, std::move(dep));
  scheduler.register_phase(commit_clock);
  Schedule_factory schedule_factory(clock, commit_clock);
  Task_sequencer gen;

  std::atomic<bool> phase_zero_entered{false};
  std::atomic<bool> release_phase_zero{false};
  std::atomic<bool> stop_requested{false};
  std::atomic<bool> phase_zero_finished{false};
  std::atomic<bool> phase_one_executed{false};

  Task_stop_while_phase_zero_running task(
      phase_zero_entered, release_phase_zero, stop_requested,
      phase_zero_finished, phase_one_executed);

  ASSERT_TRUE(scheduler.enqueue(schedule_factory.create(gen.next_id(), 0, true),
                                std::move(task)));

  phase_zero_entered.wait(false);

  stop_requested.store(true);
  scheduler.stop_now();

  auto sync_future = std::async(std::launch::async, [&scheduler]() {
    return scheduler.synchronize(true);
  });

  auto sync_status = sync_future.wait_for(std::chrono::milliseconds(50));

  release_phase_zero.store(true);
  release_phase_zero.notify_all();
  phase_zero_finished.wait(false);

  EXPECT_EQ(sync_status, std::future_status::timeout);
  EXPECT_EQ(sync_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  EXPECT_TRUE(sync_future.get());
  EXPECT_FALSE(phase_one_executed.load());

  scheduler.deinit();
  th_pool->end_execution();
  th_pool.reset();
}

TEST(SchedulerPhases, StopNowWaitsForAllRunningPhaseZeroTasks) {
  constexpr std::size_t task_count = 2;

  std::ignore = Statistics_map::init_statistics(0);
  auto th_pool = std::make_shared<Thread_pool<Task_result>>(task_count);
  if (th_pool->init()) {
    GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
  }
  Scheduler_clock_ptr clock = std::make_shared<Clock_lwm_registry>();
  Scheduler_clock_ptr commit_clock = std::make_shared<Commit_order_clock>();
  Dependency_tracker_ptr dep = std::make_unique<Dependency_tracker_stub>();
  Scheduler scheduler(th_pool, clock, std::move(dep));
  scheduler.register_phase(commit_clock);
  Schedule_factory schedule_factory(clock, commit_clock);
  Task_sequencer gen;

  std::array<std::atomic<bool>, task_count> phase_zero_entered{};
  std::array<std::atomic<bool>, task_count> release_phase_zero{};
  std::array<std::atomic<bool>, task_count> phase_zero_finished{};
  std::array<std::atomic<bool>, task_count> phase_one_executed{};
  std::atomic<bool> stop_requested{false};

  for (std::size_t task_index = 0; task_index < task_count; ++task_index) {
    Task_stop_while_phase_zero_running_many<task_count> task(
        task_index, phase_zero_entered, release_phase_zero, stop_requested,
        phase_zero_finished, phase_one_executed);

    ASSERT_TRUE(scheduler.enqueue(
        schedule_factory.create(gen.next_id(), 0, true), std::move(task)));
  }

  for (auto &entered : phase_zero_entered) {
    entered.wait(false);
  }

  stop_requested.store(true);
  scheduler.stop_now();

  auto sync_future = std::async(std::launch::async, [&scheduler]() {
    return scheduler.synchronize(true);
  });

  EXPECT_EQ(sync_future.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout);

  release_phase_zero[0].store(true);
  release_phase_zero[0].notify_all();
  phase_zero_finished[0].wait(false);

  EXPECT_EQ(sync_future.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout);

  release_phase_zero[1].store(true);
  release_phase_zero[1].notify_all();
  phase_zero_finished[1].wait(false);

  EXPECT_EQ(sync_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  EXPECT_TRUE(sync_future.get());

  for (auto &phase_one_flag : phase_one_executed) {
    EXPECT_FALSE(phase_one_flag.load());
  }

  scheduler.deinit();
  th_pool->end_execution();
  th_pool.reset();
}

}  // namespace mysql::scheduler
