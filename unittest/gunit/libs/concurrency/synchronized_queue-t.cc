// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <functional>
#include <random>
#include <sstream>
#include <thread>
#include <vector>
#include "mysql/concurrency/locking_queue.h"
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/concurrency/thread.h"

namespace mysql::concurrency {

using Class_type = std::vector<uint32_t>;
using Elem_type = std::shared_ptr<Class_type>;

// R2. Synchronized queue shall provide mutual exclusion mechanism for
//     selected number of consumers
// 1. Specify workload
// 2. Create producer thread
// 3. Create "num_consumers" - N consumer threads
// 4. Wait until consumer threads finished processing
// 5. Check that consumed data is as expected
// Note: specified workload may be not enough to feed selected number of
// consumers
template <class Queue_type>
void test_queue(std::size_t num_consumers) {
  std::vector<concurrency::Thread> threads;

  using random_bytes_engine =
      std::independent_bits_engine<std::default_random_engine,
                                   CHAR_BIT * sizeof(uint32_t), uint32_t>;

  random_bytes_engine rbe;

  // prepare producer:

  std::size_t num_elems = 5000;

  Queue_type q;
  std::atomic<bool> done_producing = false;
  std::atomic<bool> done_consuming = false;

  std::size_t bytes_num = 50000;
  std::vector<uint32_t> pattern(bytes_num);
  std::generate(pattern.begin(), pattern.end(), std::ref(rbe));
  pattern[0] = 0;
  std::set<std::size_t> elem_ids;

  threads.emplace_back(
      [&](size_t) -> void {
        // prepare data
        std::size_t iter = 0;
        while (iter < num_elems) {
          Elem_type elem(new std::vector<uint32_t>(bytes_num));
          std::copy(pattern.begin(), pattern.end(), elem->begin());
          elem->operator[](0) = iter;
          q.enqueue(std::move(elem));
          elem_ids.insert(iter);
          ++iter;
        }
        done_producing.store(true);
      },
      0);

  std::vector<uint32_t> output(bytes_num * num_elems);

  // prepare consumers:

  std::atomic<std::size_t> processed = 0;

  for (std::size_t idc = 0; idc < num_consumers; ++idc) {
    threads.emplace_back(
        [&](size_t) -> void {
          while (!done_consuming.load()) {
            auto [elem, obtained] =
                q.dequeue([&]() -> bool { return done_consuming.load(); });
            if (obtained) {
              std::size_t shift = (elem->operator[](0)) * bytes_num;
              std::copy(elem->begin(), elem->end(), output.begin() + shift);
              ++processed;
              auto val = processed.load();
              if (val == num_elems) {
                done_consuming.store(true);
              }
            }
          }
        },
        idc);
  }

  // clean up:

  while (!done_consuming.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // release consumers
  q.notify_all();

  for (std::size_t idx = 0; idx < threads.size(); ++idx) {
    threads[idx].join();
  }

  // verify data:
  for (std::size_t iter = 0; iter < num_elems; ++iter) {
    std::size_t shift = iter * bytes_num;
    EXPECT_TRUE(0 == std::memcmp(output.data() + shift + 1, pattern.data() + 1,
                                 (bytes_num - 1) * sizeof(uint32_t)));
    elem_ids.erase(output[shift]);
  }
  EXPECT_TRUE(elem_ids.empty());
}

struct Temp_struct {
  Temp_struct() = default;
  Temp_struct(Temp_struct &&src) = default;
  Temp_struct &operator=(Temp_struct &&src) noexcept = default;
  Temp_struct &operator=(const Temp_struct &) = delete;
  Temp_struct(const Temp_struct &) = delete;
  std::shared_ptr<int> m_data;
  int a{6};
};

// Test that objects are correctly moved into and from synced queue (no dangling
// references that prevent internal object from being destructed)
TEST(SynchronizedQueue, SyncBoundedQueueSanity) {
  Sync_bounded_queue<Temp_struct, 2> queue;
  std::shared_ptr<int> shared_data = std::make_shared<int>();
  *shared_data = 500;

  Temp_struct struct_obj;
  struct_obj.m_data = shared_data;
  struct_obj.a = 200;
  {
    queue.enqueue(std::move(struct_obj));
    // struct_obj is no longer valid
    auto struct_obj_obtained = queue.dequeue([&]() -> bool { return false; });
    ASSERT_EQ(struct_obj_obtained.second, true);
    ASSERT_EQ(*(struct_obj_obtained.first.m_data), *shared_data);
    ASSERT_EQ(struct_obj_obtained.first.a, 200);
    ASSERT_EQ((struct_obj_obtained.first.m_data).use_count(), 2);
    ASSERT_EQ(shared_data.use_count(), 2);
  }
  ASSERT_EQ(shared_data.use_count(), 1);
  shared_data.reset();
  ASSERT_EQ(shared_data.use_count(), 0);
}

// Locking_queue test for 1 consumer
TEST(SynchronizedQueue, SyncSimpleQueue1) {
  test_queue<Locking_queue<Elem_type>>(1);
}

// Locking_queue test for 2 consumers
TEST(SynchronizedQueue, SyncSimpleQueue2) {
  test_queue<Locking_queue<Elem_type>>(2);
}

// Locking_queue test for 4 consumers
TEST(SynchronizedQueue, SyncSimpleQueue4) {
  test_queue<Locking_queue<Elem_type>>(4);
}

// Locking_queue test for 8 consumers
TEST(SynchronizedQueue, SyncSimpleQueue8) {
  test_queue<Locking_queue<Elem_type>>(8);
}

// Locking_queue test for 16 consumers
TEST(SynchronizedQueue, SyncSimpleQueue16) {
  test_queue<Locking_queue<Elem_type>>(16);
}

// Locking_queue test for 32 consumers
TEST(SynchronizedQueue, SyncSimpleQueue32) {
  test_queue<Locking_queue<Elem_type>>(32);
}

TEST(SynchronizedQueue, SyncBoundedQueue1) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(1);
}

TEST(SynchronizedQueue, SyncBoundedQueue2) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(2);
}

TEST(SynchronizedQueue, SyncBoundedQueue4) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(4);
}

TEST(SynchronizedQueue, SyncBoundedQueue8) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(8);
}

TEST(SynchronizedQueue, SyncBoundedQueue16) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(16);
}

TEST(SynchronizedQueue, SyncBoundedQueue32) {
  test_queue<Sync_bounded_queue<Elem_type, 512>>(32);
}

}  // namespace mysql::concurrency
