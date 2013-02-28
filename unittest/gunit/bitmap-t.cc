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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  bitmap.intersect(0xBBBB);
  EXPECT_TRUE(bitmap.is_set(0));
  EXPECT_TRUE(bitmap.is_set(1));
  EXPECT_FALSE(bitmap.is_set(2));
  EXPECT_TRUE(bitmap.is_set(3));
  bitmap.clear_bit(0);
  bitmap.clear_bit(1);
  bitmap.clear_bit(3);
  EXPECT_TRUE(bitmap.is_clear_all());
}

TEST_F(BitmapTest, UULTest)
{
  bitmap.set_all();
  bitmap.intersect(0x0123456789ABCDEF);
  ulonglong uul= bitmap.to_ulonglong();
  EXPECT_TRUE(uul == 0x0123456789ABCDEF);

  Bitmap<24> bitmap24;
  bitmap24.init();
  bitmap24.set_all();
  bitmap24.intersect(0x47B);
  ulonglong uul24= bitmap24.to_ulonglong();
  EXPECT_TRUE(uul24 == 0x47B);
}

}  // namespace

