/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
#include <stddef.h>

#include "my_thread.h"

namespace my_thread_unittest {

extern "C" void *handle_thread(void *) {
  my_thread_exit(nullptr);
  return nullptr;  // Avoid compiler warning
}

class ThreadTest : public ::testing::Test {};

// Join with running/already finished thread
TEST(Thread, CreateAndJoin) {
  my_thread_handle thr;
  my_thread_attr_t thr_attr;
  my_thread_attr_init(&thr_attr);
#ifdef _WIN32
  const HANDLE null_thr_handle = nullptr;
#endif
  int ret, tries = 10;
  while (tries) {
    ret = my_thread_create(&thr, &thr_attr, handle_thread, nullptr);
    EXPECT_EQ(0, ret);
#ifdef _WIN32
    EXPECT_NE(null_thr_handle, thr.handle);
#endif
    ret = my_thread_join(&thr, nullptr);
    EXPECT_EQ(0, ret);
    tries--;
  }
  my_thread_attr_destroy(&thr_attr);
}

}  // namespace my_thread_unittest
