/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/containers/atomics_array_index_interleaved.h"
#include "sql/containers/integrals_lockfree_queue.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace containers {
namespace lf {
namespace unittests {

static std::atomic<int> pushed{0};
static std::atomic<int> popped{0};
static std::atomic<int> removed{0};

class Integrals_lockfree_queue_test : public ::testing::Test {
 public:
  using value_type = long;

  static constexpr int Threads = 8;
  static constexpr value_type Null = -1;
  static constexpr value_type Erased = -2;
  static constexpr value_type Workload = 32;

  template <typename Queue>
  static void test_queue(Queue &queue) {
    for (value_type idx = 0; idx != Workload + 1; ++idx) {
      queue.push(idx);
    }
    EXPECT_EQ(queue.array().to_string(),
              "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, "
              "18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, EOF");
    EXPECT_EQ(queue.to_string(),
              "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, "
              "18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, EOF");
    EXPECT_EQ(queue.array().find(31), 31);
    EXPECT_EQ(queue.is_full(), true);
    EXPECT_EQ(queue.front(), 0);
    EXPECT_EQ(queue.back(), 31);
    EXPECT_EQ(queue.head(), 0);
    EXPECT_EQ(queue.tail(), 32);
    EXPECT_EQ(queue.get_state(),
              Queue::enum_queue_state::
                  NO_SPACE_AVAILABLE);  // Pushing one more item, above the
                                        // capacity, makes the queue state to
                                        // report no more space

    for (value_type idx = 0; idx != Workload / 2; ++idx) {
      queue.pop();
    }

    for (value_type idx = 0;
         queue.get_state() != Queue::enum_queue_state::NO_SPACE_AVAILABLE;
         ++idx) {
      queue.push(idx);
    }
    EXPECT_EQ(queue.head(), 16);
    EXPECT_EQ(queue.tail(), 48);
    EXPECT_EQ(queue.array().to_string(),
              "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, "
              "18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, EOF");
    EXPECT_EQ(queue.to_string(),
              "16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, "
              "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, EOF");
    EXPECT_EQ(queue.array().find(31), 31);

    queue.clear();
    queue.pop();
    EXPECT_EQ(
        queue.get_state(),
        Queue::enum_queue_state::NO_MORE_ELEMENTS);  // Poping on more item than
                                                     // the ones available maked
                                                     // the queue state to
                                                     // report no more elements

    pushed = 0;
    popped = 0;
    removed = 0;

    size_t total_threads{Threads * 4};
    std::vector<std::thread> threads;

    for (size_t idx = 0; idx != total_threads; ++idx) {
      threads.emplace_back(
          [&](int n_thread) -> void {
            switch (n_thread % 4) {
              case 0: {  // Producer threads
                for (size_t k = 0; k != Workload;) {
                  value_type value = n_thread * Workload + k;
                  queue << value;  // Using `push` or `<<` is the same
                  if (queue.get_state() == Queue::enum_queue_state::SUCCESS) {
                    ++pushed;
                    ++k;
                  }
                  std::this_thread::yield();
                }
                break;
              }
              case 1: {  // Remover thread
                for (; popped.load() + removed.load() != Workload * Threads;) {
                  int n_success =
                      queue.erase_if([=](value_type item) mutable -> bool {
                        return item >= ((n_thread - 1) * Workload) &&
                               item < (n_thread * Workload);
                      });
                  if (n_success > 0) {
                    removed += n_success;
                  }
                  std::this_thread::yield();
                }
                break;
              }
              case 2: {  // Finder thread
                size_t start{0};
                for (; popped.load() + removed.load() != Workload * Threads;) {
                  value_type found{Null};
                  std::tie(found, start) = queue.array().find_if(
                      [=](value_type item, size_t) mutable -> bool {
                        return item >= ((n_thread - 2) * Workload) &&
                               item < ((n_thread - 1) * Workload);
                      },
                      start);
                  if (start == queue.capacity()) {
                    start = 0;
                  }
                  std::this_thread::yield();
                }
                break;
              }
              case 3: {  // Consumer thread
                for (; popped.load() + removed.load() != Workload * Threads;) {
                  value_type value{Null};
                  queue >> value;  // Using `pop` or `>>` is the same
                  if (queue.get_state() == Queue::enum_queue_state::SUCCESS) {
                    ++popped;
                  }
                  std::this_thread::yield();
                }
              }
            }
          },
          idx);
    }

    for (size_t idx = 0; idx != total_threads; ++idx) {
      threads[idx].join();
    }

    EXPECT_EQ(pushed.load(), Workload * Threads);
    EXPECT_EQ(popped.load() + removed.load(), pushed.load());
  }

 protected:
  Integrals_lockfree_queue_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Integrals_lockfree_queue_test, Padding_indexing_test) {
  size_t size = Workload;
  container::Integrals_lockfree_queue<Integrals_lockfree_queue_test::value_type,
                                      Null, Erased>
      queue{size};
  EXPECT_EQ(queue.capacity(), size);
  EXPECT_EQ(queue.allocated_size(),
            queue.capacity() * memory::cache_line_size());
  Integrals_lockfree_queue_test::test_queue(queue);
}

TEST_F(Integrals_lockfree_queue_test, Interleaved_indexing_test) {
  size_t size = Workload;
  container::Integrals_lockfree_queue<
      Integrals_lockfree_queue_test::value_type, Null, Erased,
      container::Interleaved_indexing<
          Integrals_lockfree_queue_test::value_type>>
      queue{size};
  EXPECT_EQ(queue.capacity(), size);
  EXPECT_EQ(queue.allocated_size(),
            queue.capacity() *
                sizeof(std::atomic<Integrals_lockfree_queue_test::value_type>));
  Integrals_lockfree_queue_test::test_queue(queue);
}

}  // namespace unittests
}  // namespace lf
}  // namespace containers
