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

#include <algorithm>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "mysql/harness/mpmc_queue.h"
#include "mysql/harness/mpsc_queue.h"

template <typename T>
class TestProducerConsumerQueue : public ::testing::Test {};

// a move-only type to check the queue moves objects if requested
template <class T>
class MoveOnly {
 public:
  using value_type = T;
  MoveOnly() : v_{} {}
  MoveOnly(value_type v) : v_{v} {}

  MoveOnly(const MoveOnly &) = delete;
  MoveOnly &operator=(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) = default;
  MoveOnly &operator=(MoveOnly &&) = default;

  operator value_type() const { return v_; }

 private:
  value_type v_;
};

using ProducerConsumerQueueTypes =
    ::testing::Types<mysql_harness::MPMCQueue<int>,
                     mysql_harness::MPSCQueue<int>,
                     mysql_harness::MPMCQueue<MoveOnly<int>>,
                     mysql_harness::MPSCQueue<MoveOnly<int>>>;
TYPED_TEST_SUITE(TestProducerConsumerQueue, ProducerConsumerQueueTypes);

/**
 * @test
 *       ensure a simple push doesn't block
 */
TYPED_TEST(TestProducerConsumerQueue, push) {
  mysql_harness::WaitingQueueAdaptor<TypeParam> q;

  q.push(1);
}

/**
 * @test
 *       ensure a pop() returns the value that got pushed
 */
TYPED_TEST(TestProducerConsumerQueue, pop) {
  mysql_harness::WaitingQueueAdaptor<TypeParam> q;

  q.push(1);

  EXPECT_EQ(q.pop(), 1);
}

/**
 * @test
 *       ensure try_pop doesn't block on empty queue
 */
TYPED_TEST(TestProducerConsumerQueue, try_pop) {
  mysql_harness::WaitingQueueAdaptor<TypeParam> q;

  q.push(1);

  typename TypeParam::value_type item = 0;
  EXPECT_EQ(q.try_pop(item), true);
  EXPECT_EQ(item, 1);

  SCOPED_TRACE("// queue is empty, item shouldn't change");
  item = 0;
  EXPECT_EQ(q.try_pop(item), false);
  EXPECT_EQ(item, 0);
}

/**
 * @test
 *       ensure a simple push doesn't block
 */
TYPED_TEST(TestProducerConsumerQueue, enqueue) {
  TypeParam q;

  EXPECT_TRUE(q.enqueue(1));
}

/**
 * @test
 *       ensure a pop() returns the value that got pushed
 */
TYPED_TEST(TestProducerConsumerQueue, dequeue) {
  TypeParam q;

  EXPECT_TRUE(q.enqueue(1));

  typename TypeParam::value_type d;
  EXPECT_TRUE(q.dequeue(d));
}

class TestProducerConsumerQueueP
    : public ::testing::TestWithParam<std::tuple<unsigned int, unsigned int>> {
};

/**
 * @test
 *       ensure concurrent pop/push don't trash the queue
 */
TEST_P(TestProducerConsumerQueueP, mpmc) {
  mysql_harness::WaitingMPMCQueue<int> q;
  const unsigned int total_rounds = 16 * 1024;

  const unsigned int num_producers = std::get<0>(GetParam());
  unsigned int rounds_for_consumers = total_rounds;
  const unsigned int rounds_per_producer = total_rounds / num_producers;

  const unsigned int num_consumers = std::get<1>(GetParam());
  unsigned int rounds_for_producers = total_rounds;
  const unsigned int rounds_per_consumer = total_rounds / num_consumers;

  std::vector<std::thread> consumers;
  std::vector<std::thread> producers;

  for (unsigned int i = 0; i < num_consumers; i++) {
    unsigned int rounds_this_consumer =
        std::min(rounds_per_consumer, rounds_for_consumers);

    consumers.emplace_back(
        [&q](unsigned int rounds) {
          while (rounds-- > 0) {
            EXPECT_EQ(q.pop(), 42);
          }
        },
        rounds_this_consumer);

    rounds_for_consumers -= rounds_this_consumer;
  }

  for (unsigned int i = 0; i < num_producers; i++) {
    unsigned int rounds_this_producer =
        std::min(rounds_per_producer, rounds_for_producers);

    producers.emplace_back(
        [&q](unsigned int rounds) {
          while (rounds-- > 0) {
            q.push(42);
          }
        },
        rounds_this_producer);

    rounds_for_producers -= rounds_this_producer;
  }

  // wait for all threads to shutdown
  for (auto &producer : producers) {
    producer.join();
  }

  // ... and all consumers
  for (auto &consumer : consumers) {
    consumer.join();
  }

  // the queue should be empty
  int last_item = 0;

  EXPECT_EQ(q.try_pop(last_item), false);
}

// ::testing::Combine() would be nice, but doesn't work with sun-cc
INSTANTIATE_TEST_SUITE_P(
    ManyToMany, TestProducerConsumerQueueP,
    ::testing::Values(
        std::make_tuple(1, 1), std::make_tuple(1, 2), std::make_tuple(1, 4),
        std::make_tuple(1, 8), std::make_tuple(1, 16), std::make_tuple(2, 1),
        std::make_tuple(2, 2), std::make_tuple(2, 4), std::make_tuple(2, 8),
        std::make_tuple(2, 16), std::make_tuple(4, 1), std::make_tuple(4, 2),
        std::make_tuple(4, 4), std::make_tuple(4, 8), std::make_tuple(4, 16),
        std::make_tuple(8, 1), std::make_tuple(8, 2), std::make_tuple(8, 4),
        std::make_tuple(8, 8), std::make_tuple(8, 16), std::make_tuple(16, 1),
        std::make_tuple(16, 2), std::make_tuple(16, 4), std::make_tuple(16, 8),
        std::make_tuple(16, 16)),
    [](testing::TestParamInfo<std::tuple<unsigned int, unsigned int>> p)
        -> std::string {
      return "p" + std::to_string(std::get<0>(p.param)) + "_" + "c" +
             std::to_string(std::get<1>(p.param));
    });

class TestProducerConsumerQueueSCP
    : public ::testing::TestWithParam<std::tuple<unsigned int, unsigned int>> {
};

/**
 * @test
 *       ensure concurrent pop/push don't trash the queue
 */
TEST_P(TestProducerConsumerQueueSCP, mpsc) {
  mysql_harness::WaitingMPSCQueue<int> q;
  const unsigned int total_rounds = 16 * 1024;

  const unsigned int num_producers = std::get<0>(GetParam());
  unsigned int rounds_for_consumers = total_rounds;
  const unsigned int rounds_per_producer = total_rounds / num_producers;

  const unsigned int num_consumers = std::get<1>(GetParam());
  unsigned int rounds_for_producers = total_rounds;
  const unsigned int rounds_per_consumer = total_rounds / num_consumers;

  std::vector<std::thread> consumers;
  std::vector<std::thread> producers;

  for (unsigned int i = 0; i < num_consumers; i++) {
    unsigned int rounds_this_consumer =
        std::min(rounds_per_consumer, rounds_for_consumers);

    consumers.emplace_back(
        [&q](unsigned int rounds) {
          while (rounds-- > 0) {
            EXPECT_EQ(q.pop(), 42);
          }
        },
        rounds_this_consumer);

    rounds_for_consumers -= rounds_this_consumer;
  }

  for (unsigned int i = 0; i < num_producers; i++) {
    unsigned int rounds_this_producer =
        std::min(rounds_per_producer, rounds_for_producers);

    producers.emplace_back(
        [&q](unsigned int rounds) {
          while (rounds-- > 0) {
            q.push(42);
          }
        },
        rounds_this_producer);

    rounds_for_producers -= rounds_this_producer;
  }

  // wait for all threads to shutdown
  for (auto &producer : producers) {
    producer.join();
  }

  // ... and all consumers
  for (auto &consumer : consumers) {
    consumer.join();
  }

  // the queue should be empty
  int last_item = 0;

  EXPECT_EQ(q.try_pop(last_item), false);
}

// ::testing::Combine() would be nice, but doesn't work with sun-cc
INSTANTIATE_TEST_SUITE_P(
    ManyToSingle, TestProducerConsumerQueueSCP,
    ::testing::Values(std::make_tuple(1, 1), std::make_tuple(2, 1),
                      std::make_tuple(4, 1), std::make_tuple(8, 1),
                      std::make_tuple(16, 1)),
    [](testing::TestParamInfo<std::tuple<unsigned int, unsigned int>> p)
        -> std::string {
      return "p" + std::to_string(std::get<0>(p.param)) + "_" + "c" +
             std::to_string(std::get<1>(p.param));
    });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
