/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include <thread>

#include <gmock/gmock.h>

using namespace std::chrono_literals;

template <class T>
class MoveOnly {
 public:
  using value_type = T;

  MoveOnly(value_type v) : v_{v} {}

  MoveOnly(const MoveOnly &rhs) = delete;
  MoveOnly &operator=(const MoveOnly &rhs) = delete;

  MoveOnly(MoveOnly &&rhs) = default;
  MoveOnly &operator=(MoveOnly &&rhs) = default;

  value_type value() const { return v_; }
  void value(value_type v) { v_ = v; }

 private:
  value_type v_;
};

TEST(Monitor, trivial) {
  Monitor<int> m{1};

  EXPECT_EQ(1, m([](auto &v) { return v; }));
}

TEST(Monitor, move_only) {
  Monitor<MoveOnly<int>> m{1};

  EXPECT_EQ(1, m([](auto &v) { return v.value(); }));
}

TEST(WaitableMonitor, move_only) {
  WaitableMonitor<MoveOnly<int>> m{0};

  EXPECT_EQ(0, m([](const auto &v) { return v.value(); }));

  std::thread thr([&m]() {
    m.serialize_with_cv([](auto &v, auto &cv) {
      v.value(1);
      cv.notify_one();
    });
  });

  m.wait_for(100ms, [](const auto &v) { return v.value() == 1; });

  EXPECT_EQ(1, m([](const auto &v) { return v.value(); }));

  thr.join();
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
