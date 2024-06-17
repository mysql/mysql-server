/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <thread>

#include "helper/plugin_monitor.h"
#include "mock/mock_plugin_state.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;
using testing::Values;
using testing::WithParamInterface;

using namespace helper;
using namespace mysql_harness;

const char *k_service_first = "plugin_name 1";
const char *k_service_second = "plugin_name 2";
const char *k_service_third = "plugin_name 3";
const char *k_service_other1 = "some_service 1";
const char *k_service_other2 = "some_service 2";
const char *k_service_other3 = "some_service 3";

class PluginMonitorTests : public Test {
 public:
  using ObserverId = PluginState::ObserverId;
  using ObserverPtr = PluginState::ObserverPtr;
  using ObserverSharedPtr = std::shared_ptr<PluginStateObserver>;
  void make_sut() { sut_.reset(new PluginMonitor(&mock_plugin_state_)); }
  void free_sut() { sut_.reset(); }

  class MustBeTrue {
   public:
    bool operator()(bool value) const { return value; }
  };

  class MustBeFalse {
   public:
    bool operator()(bool value) const { return !value; }
  };

  /**
   * Check atomic variable with waiting for the change..
   *
   * Default value of `Op` template parameter means "wait for true".
   */
  template <typename Op = MustBeTrue>
  void check_thread_state_is(std::atomic<bool> &state,
                             const uint32_t milliseconds_to_wait = 10) {
    uint32_t wait = milliseconds_to_wait;
    Op op;
    if (0 == wait) wait = 1;
    do {
      while (--wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      wait = milliseconds_to_wait;
    } while (!op(state));
  }

  MockPluginState mock_plugin_state_;
  std::unique_ptr<PluginMonitor> sut_;
};

TEST_F(PluginMonitorTests, object_register_itself_and_frees) {
  const PluginState::ObserverId id = 1000;
  EXPECT_CALL(mock_plugin_state_, push_back_observer(_)).WillOnce(Return(id));
  make_sut();
  Mock::VerifyAndClearExpectations(&mock_plugin_state_);

  EXPECT_CALL(mock_plugin_state_, remove_observer(id));
  free_sut();
  Mock::VerifyAndClearExpectations(&mock_plugin_state_);
}

using DefaultActiveServices = std::vector<std::string>;

class PluginMonitorExTest : public PluginMonitorTests,
                            public WithParamInterface<DefaultActiveServices> {
 public:
  void SetUp() override {
    default_services_ = GetParam();
    EXPECT_CALL(mock_plugin_state_, push_back_observer(_))
        .WillOnce(DoAll(Invoke([this](ObserverPtr o) {
                          observer_ = o.lock();
                          observer_->on_begin_observation(default_services_,
                                                          {});
                        }),
                        Return(k_id_)));
    make_sut();
  }

  void TearDown() override {
    EXPECT_CALL(mock_plugin_state_, remove_observer(k_id_));
    free_sut();
  }

  DefaultActiveServices default_services_;
  ObserverSharedPtr observer_;
  const ObserverId k_id_{2000};
};

TEST_P(PluginMonitorExTest, default_service_when_no_action) {
  ASSERT_EQ(default_services_.size(), sut_->get_active_services().size());
}

TEST_P(PluginMonitorExTest, one_extra_service_when_new_plugin_reported) {
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other1);
  ASSERT_EQ(default_services_.size() + 1, sut_->get_active_services().size());

  observer_->on_plugin_shutdown(&mock_plugin_state_, k_service_other1);
  ASSERT_EQ(default_services_.size(), sut_->get_active_services().size());
}

TEST_P(PluginMonitorExTest, several_extra_services_when_new_plugin_reported) {
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other1);
  ASSERT_EQ(default_services_.size() + 1, sut_->get_active_services().size());

  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other2);
  ASSERT_EQ(default_services_.size() + 2, sut_->get_active_services().size());

  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other3);
  ASSERT_EQ(default_services_.size() + 3, sut_->get_active_services().size());

  observer_->on_plugin_shutdown(&mock_plugin_state_, k_service_other1);
  observer_->on_plugin_shutdown(&mock_plugin_state_, k_service_other2);
  observer_->on_plugin_shutdown(&mock_plugin_state_, k_service_other3);
  ASSERT_EQ(default_services_.size(), sut_->get_active_services().size());
}

TEST_P(PluginMonitorExTest, wait_for_service) {
  std::atomic<bool> running{false};
  std::atomic<bool> finished{false};
  std::thread waiting_thread{[this, &finished, &running]() {
    running = true;
    sut_->wait_for_services({k_service_other1});
    finished = true;
  }};

  check_thread_state_is(running);
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other1);
  check_thread_state_is(finished);

  waiting_thread.join();
}

TEST_P(PluginMonitorExTest, wait_for_services) {
  std::atomic<bool> running{false};
  std::atomic<bool> finished{false};
  std::thread waiting_thread{[this, &finished, &running]() {
    running = true;
    sut_->wait_for_services(
        {k_service_other1, k_service_other2, k_service_other3});
    finished = true;
  }};

  check_thread_state_is(running);
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other1);
  check_thread_state_is<MustBeFalse>(finished);
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other2);
  check_thread_state_is<MustBeFalse>(finished);
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other3);
  check_thread_state_is(finished);

  waiting_thread.join();
}

INSTANTIATE_TEST_SUITE_P(DefaultActiveServices, PluginMonitorExTest,
                         Values(DefaultActiveServices{},
                                DefaultActiveServices{k_service_first},
                                DefaultActiveServices{k_service_first,
                                                      k_service_second,
                                                      k_service_third}));

class PluginMonitorConstDefaultActiveTest : public PluginMonitorExTest {};

INSTANTIATE_TEST_SUITE_P(ConstDefaultActiveServices,
                         PluginMonitorConstDefaultActiveTest,
                         Values(DefaultActiveServices{k_service_first,
                                                      k_service_second,
                                                      k_service_third}));

TEST_P(PluginMonitorConstDefaultActiveTest, wait_for_service) {
  std::atomic<bool> running{false};
  std::atomic<bool> finished{false};
  std::thread waiting_thread{[this, &finished, &running]() {
    running = true;
    sut_->wait_for_services({k_service_first});
    finished = true;
  }};

  check_thread_state_is(running);
  check_thread_state_is(finished);

  waiting_thread.join();
}

TEST_P(PluginMonitorConstDefaultActiveTest, wait_for_services) {
  std::atomic<bool> running{false};
  std::atomic<bool> finished{false};
  std::thread waiting_thread{[this, &finished, &running]() {
    running = true;
    sut_->wait_for_services(
        {k_service_first, k_service_second, k_service_third});
    finished = true;
  }};

  check_thread_state_is(running);
  check_thread_state_is(finished);

  waiting_thread.join();
}

TEST_P(PluginMonitorConstDefaultActiveTest, wait_for_service_one_dynamic) {
  std::atomic<bool> running{false};
  std::atomic<bool> finished{false};
  std::thread waiting_thread{[this, &finished, &running]() {
    running = true;
    sut_->wait_for_services({k_service_first, k_service_other1});
    finished = true;
  }};

  check_thread_state_is(running);
  observer_->on_plugin_startup(&mock_plugin_state_, k_service_other1);
  check_thread_state_is(finished);

  waiting_thread.join();
}
