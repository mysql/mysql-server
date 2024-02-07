/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include <memory>
#include <thread>
#include <utility>

#include <gmock/gmock.h>

#include "connection.h"
#include "connection_container.h"
#include "context.h"
#include "mysql_routing_common.h"

class A {
  int x_;

 public:
  A() : x_{0} {}
  explicit A(int x) : x_(x) {}
  int get() const { return x_; }
};

class TestConcurrentMap : public testing::Test {};

/**
 * @test
 *       Verify that concurrent_map is empty when created
 */
TEST_F(TestConcurrentMap, IsMapEmptyWhenCreated) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  ASSERT_THAT(a_map.size(), testing::Eq(0u));
}

/**
 * @test
 *       Verify that concurrent_map size is increased when entry
 *       is added.
 */
TEST_F(TestConcurrentMap, IsMapSizeCorrectAfterAddedEntries) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  std::unique_ptr<A> a_1(new A);
  A *p_1 = a_1.get();
  std::unique_ptr<A> a_2(new A);
  A *p_2 = a_2.get();
  std::unique_ptr<A> a_3(new A);
  A *p_3 = a_3.get();

  a_map.put(p_1, std::move(a_1));
  a_map.put(p_2, std::move(a_2));
  a_map.put(p_3, std::move(a_3));

  ASSERT_THAT(a_map.size(), testing::Eq(3u));
}

/**
 * @test
 *       Verify that concurrent_map size is decreased when entry
 *       is removed.
 */
TEST_F(TestConcurrentMap, IsMapSizeCorrectAfterErase) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  std::unique_ptr<A> a_1(new A);
  A *p_1 = a_1.get();
  std::unique_ptr<A> a_2(new A);
  A *p_2 = a_2.get();
  std::unique_ptr<A> a_3(new A);
  A *p_3 = a_3.get();

  a_map.put(p_1, std::move(a_1));
  a_map.put(p_2, std::move(a_2));
  a_map.put(p_3, std::move(a_3));

  a_map.erase(p_1);
  a_map.erase(p_2);

  ASSERT_THAT(a_map.size(), testing::Eq(1u));
}

/**
 * @test
 *       Verify if get returns correct entry.
 */
TEST_F(TestConcurrentMap, IsGetReturnsCorrectResult) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  std::unique_ptr<A> a_1(new A(34));
  A *p_1 = a_1.get();
  std::unique_ptr<A> a_2(new A(52));
  A *p_2 = a_2.get();
  std::unique_ptr<A> a_3(new A(78));
  A *p_3 = a_3.get();

  a_map.put(p_1, std::move(a_1));
  a_map.put(p_2, std::move(a_2));
  a_map.put(p_3, std::move(a_3));

  int element_value = 0;
  auto get_value = [&element_value](const std::unique_ptr<A> &a) {
    element_value = a->get();
  };
  a_map.for_one(p_2, get_value);
  ASSERT_THAT(element_value, testing::Eq(52));
}

/**
 * @test
 *       Verify if get returns default value if entry cannot be found.
 */
TEST_F(TestConcurrentMap, IsGetReturnsDefaultValueIfNoEntriesWithKey) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  std::unique_ptr<A> a_1(new A(34));
  A *p_1 = a_1.get();
  std::unique_ptr<A> a_2(new A(52));
  A *p_2 = a_2.get();
  std::unique_ptr<A> a_3(new A(78));
  A *p_3 = a_3.get();

  a_map.put(p_1, std::move(a_1));
  a_map.put(p_2, std::move(a_2));

  int element_value = 0;
  auto get_value = [&element_value](const std::unique_ptr<A> &a) {
    element_value = a->get();
  };
  a_map.for_one(p_3, get_value);
  ASSERT_THAT(element_value, testing::Eq(0));
}

/**
 * @test
 *       Verify if function is called for every entry in concurrent_map
 *       when for_each is used.
 */
TEST_F(TestConcurrentMap, IsForEachVisitEveryEntry) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;
  std::unique_ptr<A> a_1(new A(34));
  A *p_1 = a_1.get();
  std::unique_ptr<A> a_2(new A(52));
  A *p_2 = a_2.get();
  std::unique_ptr<A> a_3(new A(78));
  A *p_3 = a_3.get();

  a_map.put(p_1, std::move(a_1));
  a_map.put(p_2, std::move(a_2));
  a_map.put(p_3, std::move(a_3));

  int counter = 0;

  auto count_function =
      [&counter](std::pair<A *const, std::unique_ptr<A>> &connection) {
        counter += connection.first->get() * connection.first->get();
      };

  a_map.for_each(count_function);
  ASSERT_THAT(counter, testing::Eq(9944));
}

/**
 * @test
 *      Verify if data in concurrent_map are not broken when many threads
 *      work concurrently with it.
 */
TEST_F(TestConcurrentMap, IsMultipleAccessCorrect) {
  concurrent_map<A *, std::unique_ptr<A>> a_map;

  auto add_to_map = [&a_map] {
    for (int i = 0; i < 1000; ++i) {
      std::unique_ptr<A> a(new A(i));
      A *p = a.get();
      a_map.put(p, std::move(a));
    }
  };

  auto list_elements = [&a_map] {
    int counter = 0;

    auto dummy_function =
        [&counter](std::pair<A *const, std::unique_ptr<A>> & /* connection */) {
          ++counter;
        };

    for (int i = 0; i < 10; ++i) {
      a_map.for_each(dummy_function);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 100; ++i) threads.emplace_back(add_to_map);
  for (int i = 0; i < 5; ++i) threads.emplace_back(list_elements);

  for (auto &each_thread : threads) each_thread.join();

  ASSERT_THAT(a_map.size(), testing::Eq(100000u));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
