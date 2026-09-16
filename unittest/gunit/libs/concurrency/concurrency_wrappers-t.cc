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
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/thread.h"

namespace mysql::concurrency {

// Here, we test new interface - check that scoped lock works on implemented
// wrapper, etc. For that, we prepare a simple mutex test
// 1. Use scoped_lock on a mutex
// 2. Try to lock, check that operation failed
// 3. Exit scope
// 4. Try to lock, check that operation succeeded
TEST(LibMysqlConcurrency, MutexBasic) {
  [[maybe_unused]] Mutex_key key{0};
  Mutex mutex_obj{MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(key)};
  {
    std::scoped_lock lock(mutex_obj);
    ASSERT_FALSE(mutex_obj.try_lock());
  }
  ASSERT_TRUE(mutex_obj.try_lock());
  mutex_obj.unlock();
}

// Here, we test new interface of conditional variable wrapper
// 1. Create three consumer threads, with the following function:
//    wait until producer is done with its job and increment counter of
//    consumers which ended their jobs
// 2. Create producer thread, which firstly notify one consumer
//    waiting on a condition variable, later on waits until the number of
//    consumers done is at least one, and in the end unblocks the rest of
//    consumers
// 3. Join all threads - test should end without a deadlock
TEST(LibMysqlConcurrency, ConditionVariable) {
  /// thanks to this atomic variable we make sure that we start consumer after
  /// producer has taken the lock
  std::atomic<bool> producer_done{false};
  std::atomic<std::size_t> consumers_done{0};

  [[maybe_unused]] Mutex_key key_mt{0};
  [[maybe_unused]] Cv_key key_cv{0};
  Mutex mutex_obj{MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(key_mt)};
  Condition_variable cv_obj{MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(key_cv)};

  auto consumer_func = [&]() -> auto{
    std::unique_lock lock(mutex_obj);
    if (!producer_done.load()) {
      cv_obj.wait(lock);
    }
    ASSERT_TRUE(producer_done.load());
    ++consumers_done;
  };

  // create three consumers
  Thread consumer_1_thread(consumer_func);
  Thread consumer_2_thread(consumer_func);
  Thread consumer_3_thread(consumer_func);

  // create a producer
  Thread producer_thread([&]() -> void {
    {
      std::scoped_lock lock(mutex_obj);
      producer_done.store(true);
    }
    cv_obj.notify_one();
    while (consumers_done.load() < 1) {
      std::this_thread::yield();
    }
    cv_obj.notify_all();
  });

  // join all threads
  producer_thread.join();
  consumer_1_thread.join();
  consumer_2_thread.join();
  consumer_3_thread.join();
}

}  // namespace mysql::concurrency
