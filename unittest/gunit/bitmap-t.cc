/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#include <algorithm>
#include <stddef.h>

#include "sql_bitmap.h"
#include "my_sys.h"

namespace bitmap_unittest {

const int BITMAP_SIZE= 128;

class BitmapTest : public ::testing::Test
{
protected:
  BitmapTest() { };

  virtual void SetUp()
  {
    bitmap.init();
  }

  Bitmap<BITMAP_SIZE> bitmap;
};

TEST_F(BitmapTest, IntersectTest)
{
  bitmap.set_prefix(4);
  bitmap.intersect(0xBBBBULL);
  EXPECT_TRUE(bitmap.is_set(0));
  EXPECT_TRUE(bitmap.is_set(1));
  EXPECT_FALSE(bitmap.is_set(2));
  EXPECT_TRUE(bitmap.is_set(3));
  bitmap.clear_bit(0);
  bitmap.clear_bit(1);
  bitmap.clear_bit(3);
  EXPECT_TRUE(bitmap.is_clear_all());
}

TEST_F(BitmapTest, ULLTest)
{
  bitmap.set_all();
  bitmap.intersect(0x0123456789ABCDEFULL);
  ulonglong ull= bitmap.to_ulonglong();
  EXPECT_TRUE(ull == 0x0123456789ABCDEFULL);

  Bitmap<24> bitmap24;
  bitmap24.init();
  bitmap24.set_all();
  bitmap24.intersect(0x47BULL);
  ulonglong ull24= bitmap24.to_ulonglong();
  EXPECT_TRUE(ull24 == 0x47BULL);
}

}  // namespace

