/*
   Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_config.h"
#include <gtest/gtest.h>

#include <my_global.h>
#include <my_pthread.h>
#include <my_bitmap.h>

namespace my_bitmap_unittest {

const uint MAX_TESTED_BITMAP_SIZE= 1024;

uint get_rand_bit(uint bitsize)
{
  return (rand() % bitsize);
}

bool test_set_get_clear_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    bitmap_set_bit(map, test_bit);
    if (!bitmap_is_set(map, test_bit))
      goto error1;
    bitmap_clear_bit(map, test_bit);
    if (bitmap_is_set(map, test_bit))
      goto error2;
  }
  return false;
error1:
  ADD_FAILURE() << "Error in set bit  bit=" << test_bit;
  return true;
error2:
  ADD_FAILURE() << "Error in clear bit  bit=" << test_bit;
  return true;
}

bool test_flip_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    bitmap_flip_bit(map, test_bit);
    if (!bitmap_is_set(map, test_bit))
      goto error1;
    bitmap_flip_bit(map, test_bit);
    if (bitmap_is_set(map, test_bit))
      goto error2;
  }
  return false;
error1:
  ADD_FAILURE() << "Error in flip bit 1  bit=" << test_bit;
  return true;
error2:
  ADD_FAILURE() << "Error in flip bit 2  bit=" << test_bit;
  return true;
}

bool test_get_all_bits(MY_BITMAP *map, uint bitsize)
{
  uint i;
  bitmap_set_all(map);
  if (!bitmap_is_set_all(map))
    goto error1;
  if (!bitmap_is_prefix(map, bitsize))
    goto error5;
  bitmap_clear_all(map);
  if (!bitmap_is_clear_all(map))
    goto error2;
  if (!bitmap_is_prefix(map, 0))
    goto error6;
  for (i=0; i<bitsize;i++)
    bitmap_set_bit(map, i);
  if (!bitmap_is_set_all(map))
    goto error3;
  for (i=0; i<bitsize;i++)
    bitmap_clear_bit(map, i);
  if (!bitmap_is_clear_all(map))
    goto error4;
  return false;
error1:
  ADD_FAILURE() << "Error in set_all";
  return true;
error2:
  ADD_FAILURE() << "Error in clear_all";
  return true;
error3:
  ADD_FAILURE() << "Error in bitmap_is_set_all";
  return true;
error4:
  ADD_FAILURE() << "Error in bitmap_is_clear_all";
  return true;
error5:
  ADD_FAILURE() << "Error in set_all through set_prefix";
  return true;
error6:
  ADD_FAILURE() << "Error in clear_all through set_prefix";
  return true;
}

bool test_compare_operators(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit1, test_bit2, test_bit3,test_bit4;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  MY_BITMAP map2_obj, map3_obj;
  MY_BITMAP *map2= &map2_obj, *map3= &map3_obj;
  my_bitmap_map map2buf[MAX_TESTED_BITMAP_SIZE];
  my_bitmap_map map3buf[MAX_TESTED_BITMAP_SIZE];
  bitmap_init(&map2_obj, map2buf, bitsize, false);
  bitmap_init(&map3_obj, map3buf, bitsize, false);
  bitmap_clear_all(map2);
  bitmap_clear_all(map3);
  for (i=0; i < no_loops; i++)
  {
    test_bit1=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit1);
    test_bit2=get_rand_bit(bitsize);
    bitmap_set_prefix(map2, test_bit2);
    bitmap_intersect(map, map2);
    test_bit3= test_bit2 < test_bit1 ? test_bit2 : test_bit1;
    bitmap_set_prefix(map3, test_bit3);
    if (!bitmap_cmp(map, map3))
      goto error1;
    bitmap_clear_all(map);
    bitmap_clear_all(map2);
    bitmap_clear_all(map3);
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit1);
    bitmap_set_prefix(map2, test_bit2);
    test_bit3= test_bit2 > test_bit1 ? test_bit2 : test_bit1;
    bitmap_set_prefix(map3, test_bit3);
    bitmap_union(map, map2);
    if (!bitmap_cmp(map, map3))
      goto error2;
    bitmap_clear_all(map);
    bitmap_clear_all(map2);
    bitmap_clear_all(map3);
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit1);
    bitmap_set_prefix(map2, test_bit2);
    bitmap_xor(map, map2);
    test_bit3= test_bit2 > test_bit1 ? test_bit2 : test_bit1;
    test_bit4= test_bit2 < test_bit1 ? test_bit2 : test_bit1;
    bitmap_set_prefix(map3, test_bit3);
    for (j=0; j < test_bit4; j++)
      bitmap_clear_bit(map3, j);
    if (!bitmap_cmp(map, map3))
      goto error3;
    bitmap_clear_all(map);
    bitmap_clear_all(map2);
    bitmap_clear_all(map3);
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit1);
    bitmap_set_prefix(map2, test_bit2);
    bitmap_subtract(map, map2);
    if (test_bit2 < test_bit1)
    {
      bitmap_set_prefix(map3, test_bit1);
      for (j=0; j < test_bit2; j++)
        bitmap_clear_bit(map3, j);
    }
    if (!bitmap_cmp(map, map3))
      goto error4;
    bitmap_clear_all(map);
    bitmap_clear_all(map2);
    bitmap_clear_all(map3);
    test_bit1=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit1);
    bitmap_invert(map);
    bitmap_set_all(map3);
    for (j=0; j < test_bit1; j++)
      bitmap_clear_bit(map3, j);
    if (!bitmap_cmp(map, map3))
      goto error5;
    bitmap_clear_all(map);
    bitmap_clear_all(map3);
  }
  return false;
error1:
  ADD_FAILURE() << "intersect error  size1=" << test_bit1
                << ",size2=" << test_bit2;
  return true;
error2:
  ADD_FAILURE() << "union error  size1=" << test_bit1
                << ",size2=" << test_bit2;
  return true;
error3:
  ADD_FAILURE() << "xor error  size1=" << test_bit1
                << ",size2=" << test_bit2;
  return true;
error4:
  ADD_FAILURE() << "subtract error  size1=" << test_bit1
                << ",size2=" << test_bit2;
  return true;
error5:
  ADD_FAILURE() << "invert error  size=" << test_bit1;
  return true;
}

bool test_count_bits_set(MY_BITMAP *map, uint bitsize)
{
  uint i, bit_count=0, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    if (!bitmap_is_set(map, test_bit))
    {
      bitmap_set_bit(map, test_bit);
      bit_count++;
    }
  }
  if (bit_count==0 && bitsize > 0)
    goto error1;
  if (bitmap_bits_set(map) != bit_count)
    goto error2;
  return false;
error1:
  ADD_FAILURE() << "No bits set";
  return true;
error2:
  ADD_FAILURE() << "Wrong count of bits set";
  return true;
}

bool test_get_first_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, test_bit= 0;
  uint no_loops= bitsize > 128 ? 128 : bitsize;

  bitmap_set_all(map);
  for (i=0; i < bitsize; i++)
    bitmap_clear_bit(map, i);
  if (bitmap_get_first_set(map) != MY_BIT_NONE)
    goto error1;
  bitmap_clear_all(map);
  for (i=0; i < bitsize; i++)
    bitmap_set_bit(map, i);
  if (bitmap_get_first(map) != MY_BIT_NONE)
    goto error2;
  bitmap_clear_all(map);

  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    bitmap_set_bit(map, test_bit);
    if (bitmap_get_first_set(map) != test_bit)
      goto error1;
    bitmap_set_all(map);
    bitmap_clear_bit(map, test_bit);
    if (bitmap_get_first(map) != test_bit)
      goto error2;
    bitmap_clear_all(map);
  }
  return false;
error1:
  ADD_FAILURE() << "get_first_set error  prefix_size=" << test_bit;
  return true;
error2:
  ADD_FAILURE() << "get_first error  prefix_size=" << test_bit;
  return true;
}

bool test_set_next_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    for (j=0; j < test_bit; j++)
      bitmap_set_next(map);
    if (!bitmap_is_prefix(map, test_bit))
      goto error1;
    bitmap_clear_all(map);
  }
  return false;
error1:
  ADD_FAILURE() << "set_next error  prefix_size=" << test_bit;
  return true;
}

bool test_get_next_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, bit_count=0, test_bit, next_count=0;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    if (!bitmap_is_set(map, test_bit))
    {
      bitmap_set_bit(map, test_bit);
      bit_count++;
    }
  }
  if (bit_count==0 && bitsize > 0)
    goto error1;
  if (bitmap_bits_set(map) != bit_count)
    goto error2;
  
  for (test_bit= bitmap_get_first_set(map);
       test_bit != MY_BIT_NONE;
       test_bit= bitmap_get_next_set(map, test_bit))
  {
    if (test_bit >= bitsize)
      goto error3;
    if (!bitmap_is_set(map, test_bit))
      goto error4;
    next_count++;
  }
  if (next_count != bit_count)
    goto error5;
  return false;
error1:
  ADD_FAILURE() << "No bits set";
  return true;
error2:
  ADD_FAILURE() << "Wrong count of bits set";
  return true;
error3:
  ADD_FAILURE() << "get_next_set out of range";
  return true;
error4:
  ADD_FAILURE() << "get_next_set bit not set";
  return true;
error5:
  ADD_FAILURE() << "Wrong count get_next_set";
  return true;
}

bool test_prefix(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    bitmap_set_prefix(map, test_bit);
    if (!bitmap_is_prefix(map, test_bit))
      goto error1;
    bitmap_clear_all(map);
    for (j=0; j < test_bit; j++)
      bitmap_set_bit(map, j);
    if (!bitmap_is_prefix(map, test_bit))
      goto error2;
    bitmap_set_all(map);
    for (j=bitsize - 1; ~(j-test_bit); j--)
      bitmap_clear_bit(map, j);
    if (!bitmap_is_prefix(map, test_bit))
      goto error3;
    bitmap_clear_all(map);
  }
  for (i=0; i < bitsize; i++)
  {
    if (bitmap_is_prefix(map, i + 1))
      goto error4;
    bitmap_set_bit(map, i);
    if (!bitmap_is_prefix(map, i + 1))
      goto error5;
    test_bit=get_rand_bit(bitsize);
    bitmap_set_bit(map, test_bit);
    if (test_bit <= i && !bitmap_is_prefix(map, i + 1))
      goto error5;
    else if (test_bit > i)
    {
      if (bitmap_is_prefix(map, i + 1))
        goto error4;
      bitmap_clear_bit(map, test_bit);
    }
  }
  return false;
error1:
  ADD_FAILURE() << "prefix1 error  prefix_size=" << test_bit;
  return true;
error2:
  ADD_FAILURE() << "prefix2 error  prefix_size=" << test_bit;
  return true;
error3:
  ADD_FAILURE() << "prefix3 error  prefix_size=" << test_bit;
  return true;
error4:
  ADD_FAILURE() << "prefix4 error  i=" << i;
  return true;
error5:
  ADD_FAILURE() << "prefix5 error  i=" << i;
  return true;
}

bool test_compare(MY_BITMAP *map, uint bitsize)
{
  MY_BITMAP map2;
  my_bitmap_map map2buf[MAX_TESTED_BITMAP_SIZE];
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  bitmap_init(&map2, map2buf, bitsize, false);

  /* Test all 4 possible combinations of set/unset bits. */
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    bitmap_clear_bit(map, test_bit);
    bitmap_clear_bit(&map2, test_bit);
    if (!bitmap_is_subset(map, &map2))
      goto error_is_subset;
    bitmap_set_bit(map, test_bit);
    if (bitmap_is_subset(map, &map2))
      goto error_is_subset;
    bitmap_set_bit(&map2, test_bit);
    if (!bitmap_is_subset(map, &map2))
      goto error_is_subset;
    bitmap_clear_bit(map, test_bit);
    if (!bitmap_is_subset(map, &map2))
      goto error_is_subset;
    /* Note that test_bit is not cleared i map2. */
  }
  bitmap_clear_all(map);
  bitmap_clear_all(&map2);
  /* Test all 4 possible combinations of set/unset bits. */
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    if (bitmap_is_overlapping(map, &map2))
      goto error_is_overlapping;
    bitmap_set_bit(map, test_bit);
    if (bitmap_is_overlapping(map, &map2))
      goto error_is_overlapping;
    bitmap_set_bit(&map2, test_bit);
    if (!bitmap_is_overlapping(map, &map2))
      goto error_is_overlapping;
    bitmap_clear_bit(map, test_bit);
    if (bitmap_is_overlapping(map, &map2))
      goto error_is_overlapping;
    bitmap_clear_bit(&map2, test_bit);
    /* Note that test_bit is not cleared i map2. */
  }
  return false;
error_is_subset:
  ADD_FAILURE() << "is_subset error";
  return true;
error_is_overlapping:
  ADD_FAILURE() << "is_overlapping error";
  return true;
}

bool test_intersect(MY_BITMAP *map, uint bitsize)
{
  uint bitsize2 = 1 + get_rand_bit(MAX_TESTED_BITMAP_SIZE - 1);
  MY_BITMAP map2;
  my_bitmap_map *map2buf= new my_bitmap_map[bitsize2];
  uint i, test_bit1, test_bit2, test_bit3;
  bitmap_init(&map2, map2buf, bitsize2, false);

  test_bit1= get_rand_bit(bitsize);
  test_bit2= get_rand_bit(bitsize);
  bitmap_set_bit(map, test_bit1);
  bitmap_set_bit(map, test_bit2);
  test_bit3= get_rand_bit(bitsize2);
  bitmap_set_bit(&map2, test_bit3);
  if (test_bit2 < bitsize2)
    bitmap_set_bit(&map2, test_bit2);

  bitmap_intersect(map, &map2);
  if (test_bit2 < bitsize2)
  {
    if (!bitmap_is_set(map, test_bit2))
      goto error;
    bitmap_clear_bit(map, test_bit2);
  }
  if (test_bit1 == test_bit3)
  {
    if (!bitmap_is_set(map, test_bit1))
      goto error;
    bitmap_clear_bit(map, test_bit1);
  }
  if (!bitmap_is_clear_all(map))
    goto error;

  bitmap_set_all(map);
  bitmap_set_all(&map2);
  for (i=0; i < bitsize2; i++)
    bitmap_clear_bit(&map2, i);
  bitmap_intersect(map, &map2);
  if (!bitmap_is_clear_all(map))
    goto error;
  delete[] map2buf;
  return false;
error:
  ADD_FAILURE() << "intersect error  bit1=" << test_bit1
                << ",bit2=" << test_bit2 << ",bit3=" << test_bit3;
  return true;
}

#if defined(GTEST_HAS_PARAM_TEST)

class BitMapTest : public ::testing::TestWithParam<uint>
{
protected:
  virtual void SetUp()
  {
    bitsize= GetParam();
    ASSERT_FALSE(bitmap_init(&map, buf, bitsize, false));
    bitmap_clear_all(&map);
  }

  MY_BITMAP map;
  my_bitmap_map buf[MAX_TESTED_BITMAP_SIZE];
  uint bitsize;
};

const uint test_values[]=
{
   1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  2*32U - 1, 2*32U, 2*32U + 1,
  3*32U - 1, 3*32U, 3*32U + 1,
  4*32U - 1, 4*32U, 4*32U + 1,
  MAX_TESTED_BITMAP_SIZE
};

INSTANTIATE_TEST_CASE_P(Foo, BitMapTest,
                        ::testing::ValuesIn(test_values));

TEST_P(BitMapTest, TestSetGetClearBit)
{
  EXPECT_FALSE(test_set_get_clear_bit(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestFlipBit)
{
  EXPECT_FALSE(test_flip_bit(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestGetAllBits)
{
  EXPECT_FALSE(test_get_all_bits(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestCompareOperators)
{
  EXPECT_FALSE(test_compare_operators(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestCountBitsSet)
{
  EXPECT_FALSE(test_count_bits_set(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestGetFirstBit)
{
  EXPECT_FALSE(test_get_first_bit(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestSetNextBit)
{
  EXPECT_FALSE(test_set_next_bit(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestGetNextBit)
{
  EXPECT_FALSE(test_get_next_bit(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestPrefix)
{
  EXPECT_FALSE(test_prefix(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestCompare)
{
  EXPECT_FALSE(test_compare(&map, bitsize)) << "bitsize=" << bitsize;
}

TEST_P(BitMapTest, TestIntersect)
{
  EXPECT_FALSE(test_intersect(&map, bitsize)) << "bitsize=" << bitsize;
}

#endif

}
