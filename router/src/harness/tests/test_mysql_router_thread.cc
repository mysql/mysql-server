/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "mysql_router_thread.h"

// this flag should be set to true by thread
std::mutex flag_cond_mutex;
std::condition_variable flag_cond;
bool flag = false;

using namespace std::chrono_literals;
constexpr auto kWaitTimeout = 1s;

class MySqlRouterThreadTest : public testing::Test {
 public:
  void SetUp() override { flag = false; }
};

void *f(void *) {
  {
    std::unique_lock<std::mutex> lk(flag_cond_mutex);
    // mutex is needed to ensure that testing thread doesn't exit
    // after a spurious wakeup after 'flag' got set, but 'notify_one()'
    // got called

    flag = true;

    flag_cond.notify_one();
  }

  return nullptr;
}

TEST_F(MySqlRouterThreadTest, ThreadCreated) {
  mysql_harness::MySQLRouterThread thread;

  ASSERT_NO_THROW(thread.run(&f, nullptr));
  {
    std::unique_lock<std::mutex> lk(flag_cond_mutex);
    ASSERT_TRUE(flag_cond.wait_for(lk, kWaitTimeout, [] { return flag; }));
  }
}

TEST_F(MySqlRouterThreadTest, DetachTreadCreated) {
  mysql_harness::MySQLRouterThread thread;

  ASSERT_NO_THROW(thread.run(&f, nullptr, true));
  {
    std::unique_lock<std::mutex> lk(flag_cond_mutex);
    ASSERT_TRUE(flag_cond.wait_for(lk, kWaitTimeout, [] { return flag; }));
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
