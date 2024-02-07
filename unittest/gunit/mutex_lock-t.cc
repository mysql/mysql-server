/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include "include/mutex_lock.h"

namespace {
struct FakeMutex {
  int times_locked = 0;
  int times_unlocked = 0;
};
void inline_mysql_mutex_lock(FakeMutex *mm, const char *, int) {
  ASSERT_TRUE(mm->times_locked == mm->times_unlocked);
  ++mm->times_locked;
}
void inline_mysql_mutex_unlock(FakeMutex *mm, const char *, int) {
  ASSERT_TRUE(mm->times_locked == mm->times_unlocked + 1);
  ++mm->times_unlocked;
}
using FakeMutexLock = Generic_mutex_lock<FakeMutex>;
}  // namespace

namespace mutex_lock_unittest {
TEST(MutexLockTest, DefaultConstruct) {
  { FakeMutexLock _; }
}
TEST(MutexLockTest, Plain) {
  FakeMutex mm;
  {
    FakeMutexLock _{&mm, __FILE__, __LINE__};
    EXPECT_EQ(1, mm.times_locked);
    EXPECT_EQ(0, mm.times_unlocked);
  }
  EXPECT_EQ(1, mm.times_locked);
  EXPECT_EQ(1, mm.times_unlocked);
}

TEST(MutexLockTest, MoveAssign) {
  FakeMutex mm;
  {
    FakeMutexLock _;
    {
      _ = FakeMutexLock{&mm, __FILE__, __LINE__};
      EXPECT_EQ(1, mm.times_locked);
      EXPECT_EQ(0, mm.times_unlocked);
    }
    // Still locked, as we move-assigned to _ which is still in scope
    EXPECT_EQ(1, mm.times_locked);
    EXPECT_EQ(0, mm.times_unlocked);
  }
  EXPECT_EQ(1, mm.times_locked);
  EXPECT_EQ(1, mm.times_unlocked);
}

TEST(MutexLockTest, StdMoveAssign) {
  FakeMutex mm;
  {
    FakeMutexLock _;
    {
      FakeMutexLock local{&mm, __FILE__, __LINE__};
      EXPECT_EQ(1, mm.times_locked);
      EXPECT_EQ(0, mm.times_unlocked);
      _ = std::move(local);
    }
    // Still locked, as we move-assigned to _ which is still in scope
    EXPECT_EQ(1, mm.times_locked);
    EXPECT_EQ(0, mm.times_unlocked);
  }
  EXPECT_EQ(1, mm.times_locked);
  EXPECT_EQ(1, mm.times_unlocked);
}

TEST(MutexLockTest, MoveConstruct) {
  FakeMutex mm;
  {
    FakeMutexLock src = FakeMutexLock(nullptr, __FILE__, __LINE__);
    FakeMutexLock dst{std::move(src)};
    EXPECT_EQ(0, mm.times_locked);
    EXPECT_EQ(0, mm.times_unlocked);
  }
  {
    FakeMutexLock src = FakeMutexLock(&mm, __FILE__, __LINE__);
    FakeMutexLock dst{std::move(src)};
    EXPECT_EQ(1, mm.times_locked);
    EXPECT_EQ(0, mm.times_unlocked);
  }
  EXPECT_EQ(1, mm.times_locked);
  EXPECT_EQ(1, mm.times_unlocked);
}

}  // namespace mutex_lock_unittest
