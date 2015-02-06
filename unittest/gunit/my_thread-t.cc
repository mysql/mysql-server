/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "my_thread.h"

namespace my_thread_unittest {

extern "C" void *handle_thread(void *arg)
{
  my_thread_exit(0);
  return 0; // Avoid compiler warning
}


class ThreadTest : public ::testing::Test
{
};


// Join with running/already finished thread
TEST(Thread, CreateAndJoin)
{
  my_thread_handle thr;
  my_thread_attr_t thr_attr;
  my_thread_attr_init(&thr_attr);
#ifdef _WIN32
  const HANDLE null_thr_handle= NULL;
#endif
  int ret, tries=10;
  while (tries)
  {
    ret= my_thread_create(&thr, &thr_attr, handle_thread, 0);
    EXPECT_EQ(0, ret);
#ifdef _WIN32
    EXPECT_NE(null_thr_handle, thr.handle);
#endif
    ret = my_thread_join(&thr, NULL);
    EXPECT_EQ(0, ret);
    tries--;
  }
  my_thread_attr_destroy(&thr_attr);
}

}  // namespace
