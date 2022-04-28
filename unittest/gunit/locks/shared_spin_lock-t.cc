/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "locks/shared_spin_lock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace lock
{
namespace unittests
{
class Shared_spin_lock_test : public ::testing::Test
{
 protected:
  Shared_spin_lock_test() {}
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Shared_spin_lock_test, Lock_unlock_test)
{
  lock::Shared_spin_lock lock1;
  lock::Shared_spin_lock lock2;

  EXPECT_EQ(lock1.acquire_exclusive().is_exclusive_acquisition(),
            true);  // Successfully acquired in exclusive mode
  EXPECT_EQ(lock2.acquire_shared().is_shared_acquisition(),
            true);  // Successfully acquired in shared mode

  EXPECT_EQ(lock1.try_shared().is_shared_acquisition(),
            false);  // Trying to acquire shared access fails
  EXPECT_EQ(lock2.try_exclusive().is_exclusive_acquisition(),
            false);  // Trying to acquire exclusive access fails
  EXPECT_EQ(lock2.try_shared().is_shared_acquisition(),
            true);  // Trying to acquire shared access succeeds

  EXPECT_EQ(lock1.release_exclusive().is_exclusive_acquisition(),
            false);  // Release and test that acquisition isn't kept
  EXPECT_EQ(lock2.release_shared().release_shared().is_shared_acquisition(),
            false);  // Release and test that acquisition isn't kept
}

TEST_F(Shared_spin_lock_test, Sentry_class_test)
{
  lock::Shared_spin_lock lock1;

  {
    lock::Shared_spin_lock::Guard sentry(
        lock1, lock::Shared_spin_lock::SL_EXCLUSIVE, true /* try and exit*/);
    EXPECT_EQ(sentry->is_exclusive_acquisition(),
              true);  // Successfully acquired in exclusive mode
  }
  EXPECT_EQ(
      lock1.is_exclusive_acquisition(),
      false);  // Successfully released the locked upon exiting the code block
  {
    lock::Shared_spin_lock::Guard sentry2(
        lock1, lock::Shared_spin_lock::SL_NO_ACQUISITION);
    sentry2.acquire(lock::Shared_spin_lock::SL_SHARED, true /* try and exit*/
    );
    EXPECT_EQ((*sentry2).is_shared_acquisition(),
              true);  // Successfully acquired in shared mode
  }
  EXPECT_EQ(
      lock1.is_shared_acquisition(),
      false);  // Successfully released the locked upon exiting the code block
}  // namespace unittests

}  // namespace unittests
}  // namespace lock
