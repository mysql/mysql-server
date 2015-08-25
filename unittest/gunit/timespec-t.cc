/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include <gtest/gtest.h>

#include "my_global.h"
#include "sql_const.h"

namespace timespec_unittest {


class TimespecTest : public ::testing::Test
{
protected:
/*
   Helper function which checks that none of the fields have overflowed.
 */
  void verify_timespec()
  {

#ifdef HAVE_STRUCT_TIMESPEC
    EXPECT_GT(ts.tv_sec, 0);
    EXPECT_GE(ts.tv_nsec, 0);
#else
    EXPECT_GT(ts.tv.i64, 0);
    EXPECT_GE(ts.max_timeout_msec, 0);
#endif
  }

  struct timespec ts;
};

/* Tests for set_timespec_nsec */

TEST_F(TimespecTest, TestNsecZero)
{
  ulonglong nsec= 0;
  set_timespec_nsec(&ts, nsec);
  verify_timespec();
}

TEST_F(TimespecTest, TestNsecMax)
{
  ulonglong nsec= 0xFFFFFFFFFFFFFFFFULL;
  set_timespec_nsec(&ts, nsec);
  verify_timespec();
}

/* Tests for set_timespec (taking a seconds argument) */

TEST_F(TimespecTest, TestSecZero)
{
  ulonglong sec= 0;
  set_timespec(&ts, sec);
  verify_timespec();
}

TEST_F(TimespecTest, TestSec_LONG_TIMEOUT)
{
  ulonglong sec= LONG_TIMEOUT;
  set_timespec(&ts, sec);
  verify_timespec();
}

TEST_F(TimespecTest, TestSec_INT_MAX32)
{
  ulonglong sec= INT_MAX32;
  set_timespec(&ts, sec);
  verify_timespec();
}

TEST_F(TimespecTest, TestSec_UINT_MAX32)
{
  ulonglong sec= UINT_MAX32;
  set_timespec(&ts, sec);
  verify_timespec();
}
}

