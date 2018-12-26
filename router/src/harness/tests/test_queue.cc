/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "queue.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"

////////////////////////////////////////
// Standard include files
#include <chrono>
#include <map>
#include <set>
#include <thread>
#include <tuple>

using mysql_harness::queue;

using std::chrono::milliseconds;
using std::get;
using std::map;
using std::ref;
using std::set;
using std::thread;

using testing::Contains;
using testing::Eq;
using testing::Not;

class TestFilledQueue : public ::testing::Test {
 public:
  void SetUp() {
    for (int i = 0; i < 10; ++i) my_queue.push(i);
    ASSERT_THAT(my_queue.empty(), Eq(false));
    ASSERT_THAT(my_queue.size(), Eq(10U));
  }

  queue<int> my_queue;
};

TEST_F(TestFilledQueue, BasicPop1) {
  for (int i = 0; i < 10; ++i) {
    auto ptr = my_queue.pop();
    EXPECT_THAT(*ptr, Eq(i));
  }
}

TEST_F(TestFilledQueue, BasicPop2) {
  for (int i = 0; i < 10; ++i) {
    int value;
    my_queue.pop(&value);
    EXPECT_THAT(value, Eq(i));
  }
}

TEST_F(TestFilledQueue, BasicPopTimeout1) {
  for (int i = 0; i < 10; ++i) {
    auto ptr = my_queue.pop(milliseconds(100));
    EXPECT_THAT(*ptr, Eq(i));
  }
  EXPECT_THAT(my_queue.pop(milliseconds(100)), Eq(nullptr));
}

TEST_F(TestFilledQueue, BasicPopTimeout2) {
  int value;
  for (int i = 0; i < 10; ++i) {
    my_queue.pop(&value, milliseconds(100));
    EXPECT_THAT(value, Eq(i));
  }
  EXPECT_THAT(my_queue.pop(&value, milliseconds(100)), Eq(false));
}

TEST_F(TestFilledQueue, BasicTryPop1) {
  for (int i = 0; i < 10; ++i) {
    auto ptr = my_queue.try_pop();
    EXPECT_THAT(*ptr, Eq(i));
  }
  EXPECT_THAT(my_queue.try_pop(), Eq(nullptr));
}

TEST_F(TestFilledQueue, BasicTryPop2) {
  int value;
  for (int i = 0; i < 10; ++i) {
    my_queue.try_pop(&value);
    EXPECT_THAT(value, Eq(i));
  }
  EXPECT_THAT(my_queue.try_pop(&value), Eq(false));
}

TEST(TestQueue, PopEmpty) {
  queue<int> my_queue;

  ASSERT_THAT(my_queue.empty(), Eq(true));
  EXPECT_THAT(my_queue.size(), Eq(0U));
  EXPECT_THAT(my_queue.try_pop(), Eq(nullptr));
  EXPECT_THAT(my_queue.pop(milliseconds(100)), Eq(nullptr));
  int value;
  EXPECT_THAT(my_queue.pop(&value, milliseconds(100)), Eq(false));
}

TEST(TestQueue, PopPush) {
  queue<int> my_queue;
  ASSERT_THAT(my_queue.empty(), Eq(true));

  auto f1 = [&my_queue] {
    int value;
    my_queue.pop(&value);
    EXPECT_THAT(value, Eq(47));
  };

  thread t1(f1);
  std::this_thread::sleep_for(milliseconds(10));
  my_queue.push(47);
  t1.join();
  EXPECT_THAT(my_queue.empty(), Eq(true));
}

TEST(TestQueue, ProducerConsumer) {
  // Don't spawn too many threads, it generates a segfault.
  thread intermediates[10];
  thread producers[50];
  thread consumers[50];
  queue<std::pair<thread::id, int>> queue[2];

  std::atomic<bool> done(false);

  // Spawn intermediate threads first
  auto intermediate_thread = [&queue, &done] {
    try {
      while (!done)
        if (auto elem = queue[0].pop(milliseconds(100))) queue[1].push(*elem);
    } catch (std::exception &err) {
      std::cerr << err.what() << std::endl;
    }
  };

  for (auto &intermediate : intermediates)
    intermediate = thread(intermediate_thread);

  auto producer_thread = [&queue, &done] {
    try {
      for (int i = 0; i < 1000; ++i)
        queue[0].push(std::make_pair(std::this_thread::get_id(), i));
    } catch (std::exception &err) {
      std::cerr << err.what() << std::endl;
    }
  };

  for (auto &producer : producers) producer = thread(producer_thread);

  auto consumer_thread = [&queue, &done] {
    map<thread::id, set<int>> seen;
    try {
      while (!done) {
        auto ptr = queue[1].pop(milliseconds(100));
        EXPECT_THAT(seen[get<0>(*ptr)], Not(Contains(get<1>(*ptr))));
        seen[get<0>(*ptr)].insert(get<1>(*ptr));
      }
    } catch (std::exception &err) {
      std::cerr << err.what() << std::endl;
    }
  };

  for (auto &consumer : consumers) consumer = thread(consumer_thread);

  // Wait for the producers to finish
  for (auto &producer : producers) producer.join();

  done = true;

  for (auto &consumer : consumers) consumer.join();

  for (auto &intermediate : intermediates) intermediate.join();
}
