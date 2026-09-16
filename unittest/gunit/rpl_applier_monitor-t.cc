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
#include "mysql/scheduler/base_dependency_tracker.h"
#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/commit_order_clock.h"
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_sequencer.h"
#include "mysql/scheduler/thread_pool.h"
#include "sql/changestreams/apply/resource/applier_channel_monitor.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/service/csa_channel.h"
#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa::unittest {

using namespace std::chrono_literals;

class RplApplierMonitorTest : public ::testing::Test {
 protected:
  std::shared_ptr<Applier_channel_monitor> monitor;

  Csa_channel csa_channel;

  class MockSessionService : public Session_service {
   public:
    MockSessionService() = default;
    bool init(std::size_t, Relay_log_info *) override { return false; }
    bool deinit() override { return false; }
    Relay_context_ptr acquire_session(scheduler::Task_id) override {
      return nullptr;
    }
    void release_session(Relay_context_ptr, scheduler::Task_id) override {}
    std::size_t get_session_number() override {
      return 8;
    }  // For allowed_unblocks = 8/4 = 2
  };

  std::atomic_flag block_task;
  std::chrono::milliseconds check_time{5};
  std::chrono::milliseconds stall_refresh_time{6};

  void SetUp() override {
    monitor = std::make_shared<Applier_channel_monitor>(csa_channel);
    block_task.clear();
    std::ignore = scheduler::Statistics_map::init_statistics(0);
    std::ignore = csa::Statistics_map::init_statistics(0, 1, false);
    auto th_pool = std::make_shared<
        scheduler::Thread_pool<scheduler::Task_result, 8192>>();
    if (th_pool->init()) {
      GTEST_SKIP() << "Not enough resources to create scheduler worker threads";
    }
    scheduler::Scheduler_clock_ptr clock =
        std::make_shared<scheduler::Clock_lwm_registry>();
    scheduler::Scheduler_clock_ptr commit_clock =
        std::make_shared<scheduler::Commit_order_clock>();
    scheduler::Dependency_tracker_ptr dep(
        new scheduler::Dependency_tracker_stub());
    auto scheduler_obj =
        std::make_shared<scheduler::Scheduler>(th_pool, clock, std::move(dep));
    auto session_service = std::make_shared<MockSessionService>();
    // Omit thread_pool assignment as not used
    csa_channel.scheduler_clock = clock;
    csa_channel.commit_order_clock = commit_clock;
    csa_channel.scheduler = scheduler_obj;
    csa_channel.session_service = session_service;
    csa_channel.channel_instance_id = 0;

    // Enqueue dummy task to have active_trx > 0
    scheduler::Schedule_factory schedule_factory(clock);
    scheduler::Task_sequencer gen;
    auto task_id = gen.next_id();
    auto sched = schedule_factory.create(task_id, 0);
    struct DummyTask {
      std::atomic_flag &block_me;
      int operator()(unsigned int, int, std::size_t) {
        block_me.wait(false);
        return 0;
      }
    };
    scheduler_obj->enqueue(sched, DummyTask{block_task}, 0, 0);

    monitor->init_monitoring(check_time);  // Short interval for testing
  }

  void TearDown() override {
    block_task.test_and_set();
    block_task.notify_one();
    if (csa_channel.scheduler != nullptr) {
      csa_channel.scheduler->synchronize();
    }
    // Cleanup if necessary
  }

  // Helper to simulate stall without triggering refresh
  void simulate_stall() { std::this_thread::sleep_for(stall_refresh_time); }

  // Helper to simulate progress
  void simulate_progress() {
    scheduler::Statistics_monitor::get(0)
        .get_stat(csa::Statistics_map::applied_events_cnt)
        .add(1, 0);
  }
};

TEST_F(RplApplierMonitorTest, AllowedUnblocksSetOnFirstCall) {
  EXPECT_EQ(monitor->get_current_unblock_counter(), 0U);
  // First call sets allowed_unblocks
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_allowed_unblocks(),
            csa_channel.session_service->get_session_number());
  EXPECT_EQ(monitor->get_current_unblock_counter(), 0U);
}

TEST_F(RplApplierMonitorTest, NoStallResetsCounter) {
  EXPECT_EQ(monitor->get_current_unblock_counter(), 0U);
  simulate_stall();
  // First call sets m_active_trx = 1, triggers potential unblock if stall
  monitor->check_applier_progress();
  monitor->check_applier_progress();
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), 1U);
  // Unblock task and let it finish, making active_trx = 0 for next call
  block_task.test_and_set();
  block_task.notify_one();
  csa_channel.scheduler->synchronize();
  std::this_thread::sleep_for(stall_refresh_time);
  // Second call with active_trx = 0, resets counter if any
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), 0U);
}

TEST_F(RplApplierMonitorTest, StallTriggersUnblockAndIncrementsCounter) {
  simulate_stall();
  // m_active_trx set to 1 by blocked task, no progress
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), 1U);
  EXPECT_EQ(monitor->get_total_unblock_counter(), 1U);
  // Counter increase confirms unblock path taken
}

TEST_F(RplApplierMonitorTest, StallUnblocksUpToLimit) {
  // First set allowed
  monitor->check_applier_progress();
  simulate_stall();
  // <= allowed allows allowed+1 increments
  size_t expected_counter = monitor->get_allowed_unblocks();
  for (size_t i = 0; i < expected_counter; ++i) {
    std::this_thread::sleep_for(stall_refresh_time);
    monitor->check_applier_progress();
  }
  EXPECT_EQ(monitor->get_current_unblock_counter(), expected_counter);
  EXPECT_EQ(monitor->get_total_unblock_counter(), expected_counter);
  // One more call should not increment
  std::this_thread::sleep_for(stall_refresh_time);
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), expected_counter);
  EXPECT_EQ(monitor->get_total_unblock_counter(), expected_counter);
}

TEST_F(RplApplierMonitorTest, ProgressAfterStallResetsCounter) {
  // First set allowed
  monitor->check_applier_progress();
  simulate_stall();
  // Stall, unblock
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), 1U);
  simulate_progress();  // Set progress
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(),
            1U);  // still one, no refresh
  std::this_thread::sleep_for(stall_refresh_time);
  // now we will refresh and reset counter
  monitor->check_applier_progress();
  EXPECT_EQ(monitor->get_current_unblock_counter(), 0U);
}

}  // namespace mysql::csa::unittest
