/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <atomic>
#include <thread>

#include "sql/locks/shared_spin_lock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace lock {
namespace unittests {

class Shared_spin_lock_test : public ::testing::Test {
 protected:
  Shared_spin_lock_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Shared_spin_lock_test, Lock_unlock_test) {
  lock::Shared_spin_lock lock1;
  lock::Shared_spin_lock lock2;
  std::atomic_flag t1_sync{true};
  std::atomic_flag t2_sync{true};

  lock1.try_exclusive();
  lock2.acquire_shared();

  std::thread s1([&]() -> void {
    t1_sync.clear();  // Unblock main thread

    EXPECT_EQ(lock1.acquire_shared().is_shared_acquisition(),
              true);  // Spin to acquire shared access until main
                      // thread releases exclusive access
    lock1.release_shared();

    t1_sync.clear();  // Unblock main thread

    EXPECT_EQ(lock2.acquire_exclusive().is_exclusive_acquisition(),
              true);  // Spin to acquire exclusive access until main
                      // thread releases shared access
    lock2.release_exclusive();
  });

  while (t1_sync.test_and_set()) std::this_thread::yield();
  lock1.release_exclusive();
  while (t1_sync.test_and_set()) std::this_thread::yield();
  lock1.acquire_exclusive();
  lock2.release_shared();

  s1.join();

  lock2.acquire_shared();

  std::thread t1([&]() -> void {
    EXPECT_EQ(lock1.try_shared().is_shared_acquisition(),
              false);  // Trying to acquire shared access fails since main
                       // thread has exclusive access

    EXPECT_EQ(lock1.try_exclusive().is_exclusive_acquisition(),
              false);  // Trying to acquire exclusive access fails since main
                       // thread has exclusive access

    t1_sync.clear();  // Unblock main thread

    EXPECT_EQ(lock1.acquire_exclusive().is_exclusive_acquisition(),
              true);  // Acquiring exclusive mode succeeds

    EXPECT_EQ(lock1.try_exclusive().is_exclusive_acquisition(),
              true);  // Trying to acquire exclusive mode while already acquired
                      // succeeds (reentrance is supported)

    EXPECT_EQ(
        lock1.release_exclusive().is_exclusive_acquisition(),
        true);  // Testing for exclusive access after releasing exclusivity is
                // still true since it was acquire twice, previously

    EXPECT_EQ(lock1.release_exclusive().is_exclusive_acquisition(),
              false);  // Testing for exclusive access after releasing
                       // exclusivity is now false
  });

  std::thread t2([&]() -> void {
    EXPECT_EQ(lock2.try_shared().is_shared_acquisition(),
              true);  // Trying to acquire shared access fails since main thread
                      // has exclusive access

    EXPECT_EQ(lock2.try_exclusive().is_exclusive_acquisition(),
              false);  // Trying to acquire exclusive access fails since main
                       // thread has exclusive access

    EXPECT_EQ(lock2.release_shared().is_shared_acquisition(),
              false);  // Testing for exclusive access after releasing share is
                       // now false

    EXPECT_EQ(lock2.try_exclusive().is_exclusive_acquisition(),
              false);  // Trying to acquire exclusive access fails since main
                       // thread has exclusive access

    t2_sync.clear();  // Unblock main thread

    EXPECT_EQ(lock2.acquire_exclusive().is_exclusive_acquisition(),
              true);  // Acquiring exclusive mode succeeds

    EXPECT_EQ(lock2.try_exclusive().is_exclusive_acquisition(),
              true);  // Trying to acquire exclusive mode while already acquired
                      // succeeds (reentrance is supported)

    EXPECT_EQ(
        lock2.release_exclusive().is_exclusive_acquisition(),
        true);  // Testing for exclusive access after releasing exclusivity is
                // still true since it was acquire twice, previously

    EXPECT_EQ(lock2.release_exclusive().is_exclusive_acquisition(),
              false);  // Testing for exclusive access after releasing
                       // exclusivity is now false
  });

  while (t1_sync.test_and_set()) std::this_thread::yield();
  lock1.release_exclusive();
  while (t2_sync.test_and_set()) std::this_thread::yield();
  lock2.release_shared();

  t1.join();
  t2.join();
}

TEST_F(Shared_spin_lock_test, Starvation_test) {
  lock::Shared_spin_lock lock;
  std::atomic_flag sync{true};

  lock.acquire_shared();
  EXPECT_EQ(lock.acquire_exclusive().is_shared_acquisition(),
            true);  // Acquiring shared mode succeeds

  std::thread t1([&]() -> void {
    EXPECT_EQ(lock.try_shared().is_shared_acquisition(),
              true);  // Acquiring shared mode succeeds even with another
                      // thread acquiring in shared mode
    lock.release_shared();
    sync.clear();  // Unblock main thread
  });

  while (sync.test_and_set()) std::this_thread::yield();

  std::thread t2([&]() -> void {
    lock.acquire_exclusive();
    EXPECT_EQ(lock.is_exclusive_acquisition(),
              true);  // Acquiring exclusive mode succeeds

    sync.clear();  // Unblock main thread
  });

  std::thread t3([&]() -> void {
    while (lock.try_shared().is_shared_acquisition()) lock.release_shared();
    EXPECT_EQ(lock.is_shared_acquisition(),
              false);  // Acquiring shared mode fails because t2 is already
                       // waiting on the exclusive lock

    sync.clear();  // Unblock main thread
  });

  while (sync.test_and_set()) std::this_thread::yield();
  lock.release_shared();
  while (sync.test_and_set()) std::this_thread::yield();

  t1.join();
  t2.join();
  t3.join();
}

TEST_F(Shared_spin_lock_test, Sentry_class_test) {
  lock::Shared_spin_lock lock1;
  std::atomic_flag t1_sync{true};
  std::atomic_flag t2_sync{true};

  std::thread t1([&]() -> void {
    lock::Shared_spin_lock::Guard sentry1{
        lock1,
        lock::Shared_spin_lock::enum_lock_acquisition::SL_NO_ACQUISITION};
    sentry1.acquire(lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED);
    EXPECT_EQ(sentry1->is_shared_acquisition(),
              true);  // Testing for shared mode access succeeds

    {
      lock::Shared_spin_lock::Guard sentry2{
          lock1,
          lock::Shared_spin_lock::enum_lock_acquisition::SL_NO_ACQUISITION};
      sentry2.acquire(
          lock::Shared_spin_lock::enum_lock_acquisition::SL_EXCLUSIVE,
          true /* try and exit*/
      );
      EXPECT_EQ((*sentry2).is_exclusive_acquisition(),
                false);  // Testing for exclusive mode access fails
    }

    t1_sync.clear();  // Unblock main thread
    while (t2_sync.test_and_set()) std::this_thread::yield();
  });

  while (t1_sync.test_and_set()) std::this_thread::yield();

  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_EXCLUSIVE,
        true /* try and exit*/};
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              false);  // Exclusivity won't be achieved until t1 exits
  }
  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED};
    EXPECT_EQ(sentry->is_shared_acquisition(),
              true);  // Shared access is allowed
  }
  t2_sync.clear();  // Allow t1 to exit

  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_EXCLUSIVE};
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              true);  // Exclusivity acquired successfullly
    {
      lock::Shared_spin_lock::Guard sentry2{
          lock1,
          lock::Shared_spin_lock::enum_lock_acquisition::SL_NO_ACQUISITION};
      sentry2.acquire(lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED,
                      true /* try and exit*/
      );
      EXPECT_EQ((*sentry2).is_shared_acquisition(),
                false);  // Testing for exclusive mode access fails
    }
  }
  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED,
        true /* try and exit*/};
    EXPECT_EQ((*sentry).is_shared_acquisition(),
              true);  // Shared mode acquired successfully
  }

  t1.join();

  EXPECT_EQ(lock1.is_exclusive_acquisition(),
            false);  // All access has been cleared
  EXPECT_EQ(lock1.is_shared_acquisition(),
            false);  // All access has been cleared

  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED};
    EXPECT_EQ(sentry->is_shared_acquisition(),
              true);  // Shared access acquired successfullly
    sentry.release();
    sentry.release();
    EXPECT_EQ(sentry->is_shared_acquisition(),
              false);  // Shared access is not acquired anymore
  }
  EXPECT_EQ(lock1.is_shared_acquisition(),
            false);  // Shared access is not acquired anymore

  lock1.acquire_shared();
  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::SL_EXCLUSIVE,
        true};  // Try to acquire in exclusive mode
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              false);  // Exclusive access isn't granted

    if (lock1.is_shared_acquisition()) {
      sentry.acquire(lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED);
      lock1.release_shared();
    }
    EXPECT_EQ(sentry->is_shared_acquisition(),
              true);  // Shared access is acquired by sentry
  }
  EXPECT_EQ(lock1.is_shared_acquisition(),
            false);  // Shared access is not acquired anymore

  {
    lock::Shared_spin_lock::Guard sentry{
        lock1, lock::Shared_spin_lock::enum_lock_acquisition::
                   SL_NO_ACQUISITION};  // Create the sentry but don't acquire
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              false);  // Exclusive access isn't granted
    EXPECT_EQ(sentry->is_shared_acquisition(),
              false);  // Shsared access isn't granted
    sentry.release();

    sentry.acquire(lock::Shared_spin_lock::enum_lock_acquisition::SL_EXCLUSIVE);
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              true);  // Exclusive access is acquired by sentry
  }
  EXPECT_EQ(lock1.is_exclusive_acquisition(),
            false);  // Exclusive access is not acquired anymore
}  // namespace unittests

}  // namespace unittests
}  // namespace lock
