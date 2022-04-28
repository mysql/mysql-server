/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/monitor.h"

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

/**
 * a simple, move-only integer type.
 */
template <class T>
class MoveOnly {
 public:
  using value_type = T;

  MoveOnly(value_type v) : v_{std::move(v)} {}

  MoveOnly(const MoveOnly &rhs) = delete;
  MoveOnly &operator=(const MoveOnly &rhs) = delete;

  MoveOnly(MoveOnly &&rhs) = default;
  MoveOnly &operator=(MoveOnly &&rhs) = default;

  const value_type &value() const { return v_; }
  value_type &value() { return v_; }
  void value(value_type v) { v_ = std::move(v); }

 private:
  value_type v_;
};

/**
 * Monitor with a trivial type.
 */
TEST(Monitor, trivial) {
  Monitor<int> m{1};

  EXPECT_EQ(1, m([](auto &v) { return v; }));
}

/**
 * Monitor with a move-only type.
 */
TEST(Monitor, move_only) {
  Monitor<MoveOnly<int>> m{1};

  EXPECT_EQ(1, m([](auto &v) { return v.value(); }));
}

/**
 * Monitor with a MoveOnly<unique_ptr<int>> type.
 */
TEST(Monitor, move_only_unique_ptr) {
  Monitor<MoveOnly<std::unique_ptr<int>>> m{std::make_unique<int>(1)};

  EXPECT_EQ(1, m([](auto &v) { return *v.value(); }));
}

/**
 * Monitor with a unique_ptr type.
 */
TEST(Monitor, unique_ptr) {
  Monitor<std::unique_ptr<int>> m{std::make_unique<int>(1)};

  EXPECT_EQ(1, m([](auto &v) { return *v; }));
}

// max time to wait for the condvar to trigger.
//
// not too small as the test will run in system which may run lots of threads
// not be woken up for a while.
//
// not too large to block the test (in case of failure) forever.
constexpr const std::chrono::milliseconds kCondVarWaitTimeout{30s};

class WaitableMonitorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<std::chrono::milliseconds, std::chrono::milliseconds>> {};

TEST_P(WaitableMonitorTest, wait_for_trivial) {
  const auto sleep_in_main_thread = std::get<0>(GetParam());
  const auto sleep_in_signal_thread = std::get<1>(GetParam());

  WaitableMonitor<int> m{0};
  EXPECT_EQ(0, m([](const auto &v) { return v; }));

  std::thread thr([&m, sleep_in_signal_thread]() {
    // sleep before notifying the cond-var to give the main thread time to enter
    // the wait.
    std::this_thread::sleep_for(sleep_in_signal_thread);

    m.serialize_with_cv([](auto &v, auto &cv) {
      v = 1;
      cv.notify_one();
    });
  });

  // sleep before entering the wait_for() to have the signal-thread some
  // advantage and signal before the call wait_for().
  std::this_thread::sleep_for(sleep_in_main_thread);

  SCOPED_TRACE("// wait " + std::to_string(kCondVarWaitTimeout.count()) +
               "ms for the thread to set the value _and_ signal readiness");
  EXPECT_TRUE(
      m.wait_for(kCondVarWaitTimeout, [](const auto &v) { return v == 1; }));

  SCOPED_TRACE("// verify that the value was set");
  EXPECT_EQ(1, m([](const auto &v) { return v; }));

  thr.join();
}

TEST_P(WaitableMonitorTest, wait_for_move_only) {
  const auto sleep_in_main_thread = std::get<0>(GetParam());
  const auto sleep_in_signal_thread = std::get<1>(GetParam());

  WaitableMonitor<MoveOnly<int>> m{0};
  EXPECT_EQ(0, m([](const auto &v) { return v.value(); }));

  std::thread thr([&m, sleep_in_signal_thread]() {
    // sleep before notifying the cond-var to give the main thread time to enter
    // the wait.
    std::this_thread::sleep_for(sleep_in_signal_thread);

    m.serialize_with_cv([](auto &v, auto &cv) {
      v.value(1);
      cv.notify_one();
    });
  });

  // sleep before entering the wait_for() to have the signal-thread some
  // advantage and signal before the call wait_for().
  std::this_thread::sleep_for(sleep_in_main_thread);

  SCOPED_TRACE("// wait " + std::to_string(kCondVarWaitTimeout.count()) +
               "ms for the thread to set the value _and_ signal readiness");
  EXPECT_TRUE(m.wait_for(kCondVarWaitTimeout,
                         [](const auto &v) { return v.value() == 1; }));

  SCOPED_TRACE("// verify that the value was set");
  EXPECT_EQ(1, m([](const auto &v) { return v.value(); }));

  thr.join();
}

INSTANTIATE_TEST_SUITE_P(Spec, WaitableMonitorTest,
                         ::testing::Values(std::make_tuple(0ms, 0ms),
                                           std::make_tuple(100ms, 0ms),
                                           std::make_tuple(0ms, 100ms)));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
