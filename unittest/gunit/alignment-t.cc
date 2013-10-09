/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "my_global.h"

#include <algorithm>
#include <vector>

namespace alignment_unittest {

/*
  Testing performance penalty of accessing un-aligned data.
  Seems to about 2% on my desktop machine.
 */
class AlignmentTest : public ::testing::Test
{
protected:
  // Increase num_iterations for actual benchmarking!
  static const int num_iterations= 1;
  static const int num_records= 100 * 1000;

  static int* aligned_data;
  static uchar* unaligned_data;

  static void SetUpTestCase()
  {
    aligned_data= new int[num_records];
    unaligned_data= new uchar[(num_records + 1) * sizeof(int)];
    for (int ix= 0; ix < num_records; ++ix)
    {
      aligned_data[ix]= ix / 10;
    }
    std::random_shuffle(aligned_data, aligned_data + num_records);
    memcpy(unaligned_data + 1, aligned_data, num_records * sizeof(int));
  }

  static void TearDownTestCase()
  {
    delete[] aligned_data;
    delete[] unaligned_data;
  }

  virtual void SetUp()
  {
    aligned_keys= new uchar* [num_records];
    unaligned_keys= new uchar* [num_records];
    for (int ix= 0; ix < num_records; ++ix)
    {
      aligned_keys[ix]=
        static_cast<uchar*>(static_cast<void*>(&aligned_data[ix]));
      unaligned_keys[ix]=
        &unaligned_data[1 + (ix * sizeof(int))];
    }
  }

  virtual void TearDown()
  {
    delete[] aligned_keys;
    delete[] unaligned_keys;
  }

  uchar **aligned_keys;
  uchar **unaligned_keys;
};

int* AlignmentTest::aligned_data;
uchar* AlignmentTest::unaligned_data;

// A copy of the generic, byte-by-byte getter.
#define sint4korrgeneric(A) (int32) (((int32) ((uchar) (A)[0])) +\
                                     (((int32) ((uchar) (A)[1]) << 8)) + \
                                     (((int32) ((uchar) (A)[2]) << 16)) + \
                                     (((int32) ((int16) (A)[3]) << 24)))
class Mem_compare_uchar_int :
  public std::binary_function<const uchar*, const uchar*, bool>
{
public:
  bool operator() (const uchar *s1, const uchar *s2)
  {
    return *(int*) s1 < *(int*) s2;
  }
};

class Mem_compare_sint4 :
  public std::binary_function<const uchar*, const uchar*, bool>
{
public:
  bool operator() (const uchar *s1, const uchar *s2)
  {
    return sint4korr(s1) < sint4korr(s2);
  }
};

class Mem_compare_sint4_generic :
  public std::binary_function<const uchar*, const uchar*, bool>
{
public:
  bool operator() (const uchar *s1, const uchar *s2)
  {
    return sint4korrgeneric(s1) < sint4korrgeneric(s2);
  }
};

#if defined(__i386__) || defined(__x86_64__) || defined(__WIN__)


TEST_F(AlignmentTest, AlignedSort)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<uchar*> keys(aligned_keys, aligned_keys + num_records);
    std::sort(keys.begin(), keys.end(), Mem_compare_uchar_int());
  }
}

TEST_F(AlignmentTest, UnAlignedSort)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<uchar*> keys(unaligned_keys, unaligned_keys + num_records);
    std::sort(keys.begin(), keys.end(), Mem_compare_uchar_int());
  }
}

TEST_F(AlignmentTest, Sint4Sort)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<uchar*> keys(unaligned_keys, unaligned_keys + num_records);
    std::sort(keys.begin(), keys.end(), Mem_compare_sint4());
  }
}

TEST_F(AlignmentTest, Sint4SortGeneric)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<uchar*> keys(unaligned_keys, unaligned_keys + num_records);
    std::sort(keys.begin(), keys.end(), Mem_compare_sint4_generic());
  }
}

#endif

}
