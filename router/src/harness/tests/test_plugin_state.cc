/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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
#include <string>
#include <vector>

#include "mysql/harness/make_shared_ptr.h"
#include "mysql/harness/plugin_state.h"

using Strings = std::vector<std::string>;
using mysql_harness::MakeSharedPtr;
using mysql_harness::PluginState;
using mysql_harness::PluginStateObserver;
using testing::_;
using testing::Eq;
using testing::Mock;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

class ObserverMock : public PluginStateObserver {
 public:
  MOCK_METHOD(void, on_begin_observation,
              (const std::vector<std::string> &active_plugins,
               const std::vector<std::string> &stopped_plugins),
              (override));
  MOCK_METHOD(void, on_end_observation, (), (override));

  MOCK_METHOD(void, on_plugin_register_waitable,
              (const PluginState *state, const std::string &name), (override));
  MOCK_METHOD(void, on_plugin_startup,
              (const PluginState *state, const std::string &name), (override));
  MOCK_METHOD(void, on_plugin_shutdown,
              (const PluginState *state, const std::string &name), (override));
};

class TestPluginState : public Test {
 public:
  using Mocks = std::vector<StrictMock<ObserverMock> *>;

  void SetUp() override {
    sut = PluginState::get_instance();

    ASSERT_EQ(0, sut->get_running_plugins().size());
    ASSERT_EQ(0, sut->get_loaded_plugins().size());
  }

  void TearDown() override {
    // Reset is part of the verification process.
    // Without the reset multiple tests running one after another will fail.
    // No separate test needed for reset functionality.
    sut->reset();
    ASSERT_EQ(0, sut->get_running_plugins().size());
    ASSERT_EQ(0, sut->get_loaded_plugins().size());
  }

  void expect_startup(const std::string &plugin_name, Mocks mocks) {
    for (auto mock : mocks) {
      EXPECT_CALL(*mock, on_plugin_startup(_, plugin_name));
    }
  }

  void expect_shutdown(const std::string &plugin_name, Mocks mocks) {
    for (auto mock : mocks) {
      EXPECT_CALL(*mock, on_plugin_shutdown(_, plugin_name));
    }
  }

  void verify_and_clean(Mocks mocks) {
    for (auto mock : mocks) {
      Mock::VerifyAndClearExpectations(mock);
    }
  }

  PluginState *sut;
};

TEST_F(TestPluginState, VerifyCountingOfStartups) {
  sut->dispatch_startup("p1");
  sut->dispatch_startup("p2");
  sut->dispatch_startup("p3");

  const Strings k_expect_all{"p1", "p2", "p3"};
  ASSERT_EQ(k_expect_all, sut->get_running_plugins());
}

TEST_F(TestPluginState, VerifyCountingOfStartupsAndShutdowns) {
  sut->dispatch_startup("p1");
  sut->dispatch_startup("p2");
  sut->dispatch_startup("p3");
  sut->dispatch_startup("p4");

  const Strings k_expect_all{"p1", "p2", "p3", "p4"};
  ASSERT_EQ(k_expect_all, sut->get_running_plugins());

  const Strings k_expect3{"p1", "p2", "p3"};
  sut->dispatch_shutdown("p4");
  ASSERT_EQ(k_expect3, sut->get_running_plugins());

  const Strings k_expect2{"p1", "p2"};
  sut->dispatch_shutdown("p3");
  ASSERT_EQ(k_expect2, sut->get_running_plugins());

  sut->dispatch_shutdown("p2");
  ASSERT_EQ(Strings({"p1"}), sut->get_running_plugins());

  sut->dispatch_shutdown("p1");
  ASSERT_EQ(Strings{}, sut->get_running_plugins());
}

TEST_F(TestPluginState, VerifyDispatchOfStartupsAndShutdowns) {
  MakeSharedPtr<StrictMock<ObserverMock>> observer1;
  MakeSharedPtr<StrictMock<ObserverMock>> observer2;
  Mocks mocks{observer1.get(), observer2.get()};

  auto sut = PluginState::get_instance();

  EXPECT_CALL(*observer1.get(), on_begin_observation(Eq(Strings{}), _));
  sut->push_back_observer(observer1.copy_base());

  EXPECT_CALL(*observer2.get(), on_begin_observation(Eq(Strings{}), _));
  sut->push_back_observer(observer2.copy_base());
  verify_and_clean(mocks);

  EXPECT_CALL(*observer1.get(), on_plugin_startup(_, StrEq("p1")));
  EXPECT_CALL(*observer2.get(), on_plugin_startup(_, StrEq("p1")));
  sut->dispatch_startup("p1");
  verify_and_clean(mocks);

  EXPECT_CALL(*observer1.get(), on_plugin_startup(_, StrEq("p2")));
  EXPECT_CALL(*observer2.get(), on_plugin_startup(_, StrEq("p2")));
  sut->dispatch_startup("p2");
  verify_and_clean(mocks);

  EXPECT_CALL(*observer1.get(), on_plugin_shutdown(_, StrEq("p1")));
  EXPECT_CALL(*observer2.get(), on_plugin_shutdown(_, StrEq("p1")));
  sut->dispatch_shutdown("p1");
  verify_and_clean(mocks);

  EXPECT_CALL(*observer1.get(), on_plugin_shutdown(_, StrEq("p2")));
  EXPECT_CALL(*observer2.get(), on_plugin_shutdown(_, StrEq("p2")));
  sut->dispatch_shutdown("p2");
  verify_and_clean(mocks);
}

TEST_F(TestPluginState, VerifyDispatchOfBeginEndObserver) {
  MakeSharedPtr<StrictMock<ObserverMock>> observer1;
  MakeSharedPtr<StrictMock<ObserverMock>> observer2;
  MakeSharedPtr<StrictMock<ObserverMock>> observer3;
  Mocks mocks{observer1.get(), observer2.get(), observer3.get()};
  auto sut = PluginState::get_instance();

  EXPECT_CALL(*observer1.get(), on_begin_observation(Eq(Strings{}), _));
  auto ob_id1 = sut->push_back_observer(observer1.copy_base());
  verify_and_clean(mocks);

  EXPECT_CALL(*observer2.get(), on_begin_observation(Eq(Strings{}), _));
  auto ob_id2 = sut->push_back_observer(observer2.copy_base());
  verify_and_clean(mocks);

  EXPECT_CALL(*observer3.get(), on_begin_observation(Eq(Strings{}), _));
  auto ob_id3 = sut->push_back_observer(observer3.copy_base());
  verify_and_clean(mocks);

  EXPECT_CALL(*observer1.get(), on_end_observation());
  sut->remove_observer(ob_id1);
  verify_and_clean(mocks);

  EXPECT_CALL(*observer2.get(), on_end_observation());
  sut->remove_observer(ob_id2);
  verify_and_clean(mocks);

  EXPECT_CALL(*observer3.get(), on_end_observation());
  sut->remove_observer(ob_id3);
  verify_and_clean(mocks);
}

TEST_F(TestPluginState, VerifyDispatchOfBeginObservationActivePluginsChanges) {
  sut->dispatch_startup("p1");

  ASSERT_EQ(Strings{"p1"}, sut->get_running_plugins());

  MakeSharedPtr<StrictMock<ObserverMock>> observer1;
  MakeSharedPtr<StrictMock<ObserverMock>> observer2;
  MakeSharedPtr<StrictMock<ObserverMock>> observer3;
  Mocks mocks{observer1.get(), observer2.get(), observer3.get()};
  auto sut = PluginState::get_instance();

  EXPECT_CALL(*observer1.get(), on_begin_observation(Eq(Strings{"p1"}), _));
  sut->push_back_observer(observer1.copy_base());
  verify_and_clean(mocks);

  expect_startup("p2", {observer1.get()});
  sut->dispatch_startup("p2");
  EXPECT_CALL(*observer2.get(),
              on_begin_observation(Eq(Strings{"p1", "p2"}), _));
  sut->push_back_observer(observer2.copy_base());
  verify_and_clean(mocks);

  expect_startup("p3", {observer1.get(), observer2.get()});
  expect_shutdown("p1", {observer1.get(), observer2.get()});
  sut->dispatch_startup("p3");
  sut->dispatch_shutdown("p1");
  EXPECT_CALL(*observer3.get(),
              on_begin_observation(Eq(Strings{"p2", "p3"}), _));
  sut->push_back_observer(observer3.copy_base());
  verify_and_clean(mocks);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
