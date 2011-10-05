/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include <gtest/gtest.h>
#include <string.h>

#include "zgroups.h"


#ifdef HAVE_UGID


void test_unsigned(int expected_len, ulonglong n)
{
  EXPECT_EQ(expected_len, Compact_coder::get_unsigned_encoded_length(n))
     << "n=" << n;
  uchar buf[Compact_coder::MAX_ENCODED_LENGTH];
  int real_len= Compact_coder::write_unsigned(buf, n);
  EXPECT_EQ(expected_len, real_len) << "n=" << n;
  Memory_reader reader(Compact_coder::MAX_ENCODED_LENGTH, buf);
  ulonglong m;
  EXPECT_EQ(READ_OK, Compact_coder::read_unsigned(&reader, &m)) << "n=" << n;
  EXPECT_EQ(n, m) << "n=" << n;
  my_off_t read_len;
  ASSERT_EQ(RETURN_STATUS_OK, reader.tell(&read_len)) << "n=" << n;
  EXPECT_EQ(expected_len, (int)read_len) << "n=" << n;
}


void test_signed(int expected_len, longlong n)
{
  EXPECT_EQ(expected_len, Compact_coder::get_signed_encoded_length(n))
     << "n=" << n;
  uchar buf[Compact_coder::MAX_ENCODED_LENGTH];
  int real_len= Compact_coder::write_signed(buf, n);
  EXPECT_EQ(expected_len, real_len) << "n=" << n;
  Memory_reader reader(Compact_coder::MAX_ENCODED_LENGTH, buf);
  longlong m;
  ASSERT_EQ(READ_OK, Compact_coder::read_signed(&reader, &m)) << "n=" << n;
  EXPECT_EQ(n, m) << "n=" << n;
  my_off_t read_len;
  ASSERT_EQ(RETURN_STATUS_OK, reader.tell(&read_len)) << "n=" << n;
  EXPECT_EQ(expected_len, (int)read_len) << "n=" << n;
}


TEST(CompactTest, UnsignedAll3Byte)
{
  ulonglong n= 0;
  // test all numbers up to and including 1 << 21
  for (; n < (1 << 7); n++)
    test_unsigned(1, n);
  for (; n < (1 << 14); n++)
    test_unsigned(2, n);
  for (; n < (1 << 21); n++)
    test_unsigned(3, n);
  // test every 101th number up to 1 << 28 (too slow to test all)
  for (; n < (1 << 28); n+= 101)
    test_unsigned(4, n);
  test_unsigned(5, n);
}


TEST(CompactTest, SignedAll3Byte)
{
  longlong n= 0;
  for (; n < (1 << 6); n++)
  {
    test_signed(1, -n);
    test_signed(1, n);
  }
  test_signed(1, -n);
  test_signed(2, n);
  n++;
  for (; n < (1 << 13); n++)
  {
    test_signed(2, -n);
    test_signed(2, n);
  }
  test_signed(2, -n);
  test_signed(3, n);
  n++;
  for (; n < (1 << 20); n++)
  {
    test_signed(3, -n);
    test_signed(3, n);
  }
  test_signed(3, -n);
  test_signed(4, n);
  n++;
  test_signed(4, -n);
  test_signed(4, n);
}


TEST(CompactTest, UnsignedAll1Bit)
{
  // test (1<<1)-1, 1<<1, (1<<2)-1, (1<<2), ..., (1<<63)-1, (1<<63)
  int len= 0;
  for (int i= 0; i < 64; i++)
  {
    ulonglong n= 1ULL << i;
    test_unsigned(max(1, len), n - 1);
    if ((i % 7) == 0)
      len++;
    test_unsigned(len, n);
  }
}


TEST(CompactTest, SignedAll1Bit)
{
  // test (1<<1)-1, 1<<1, (1<<2)-1, (1<<2), ..., (1<<62)-1, (1<<62)
  // and -(1<<1), -(1<<1)-1, -(1<<2), -(1<<2)-1, ..., -(1<<62), -(1<<62)-1
  int len= 1;
  for (int i= 0; i < 63; i++)
  {
    longlong n= 1LL << i;
    test_signed(len, -n);
    test_signed(len, n - 1);
    if ((i % 7) == 6)
      len++;
    test_signed(len, -n - 1);
    test_signed(len, n);
  }
}


/**
  Auxiliary function to generate numbers with at most N bits set.
  @param len Expected length of encoded number.
  @param max_bit_position Add bits to number only at positions before this one.
  @param todo Number of bits to generate.
  @param number Number to start bits.
*/
void generate_unsigned(int len, int max_bit_position,
                       int todo, ulonglong number)
{
  test_unsigned(len, number);
  if (todo == 0)
    return;
  for (int i= 0; i < max_bit_position; i++)
    generate_unsigned(len, i, todo - 1, number | (1ULL << i));
}


TEST(CompactTest, UnsignedAll5Bit)
{
  // test all numbers with up to 5 bits set
  int len= 0;
  for (int i= 0; i < 64; i++)
  {
    if ((i % 7) == 0)
      len++;
    generate_unsigned(len, i, 4, 1ULL << i);
  }
}


void generate_signed(int len, int max_bit_position,
                     int todo, ulonglong number)
{
  test_signed(len, number);
  test_signed(len, -number - 1);
  if (todo == 0)
    return;
  for (int i= 0; i < max_bit_position; i++)
    generate_signed(len, i, todo - 1, number | (1LL << i));
}


TEST(CompactTest, SignedAll5Bit)
{
  // test all numbers with up to 5 bits set
  int len= 1;
  for (int i= 0; i < 63; i++)
  {
    if ((i % 7) == 6)
      len++;
    generate_signed(len, i, 4, 1LL << i);
  }
}


TEST(CompactTest, UnsignedRandom)
{
  // test lots of random numbers
  srand(time(NULL));
  for (int i= 0; i < 1000000; i++)
  {
    ulonglong n= ((ulonglong)rand()) | (((ulonglong)rand()) << 32);
    test_unsigned(Compact_coder::get_unsigned_encoded_length(n), n);
  }
}


TEST(CompactTest, SignedRandom)
{
  // test lots of random numbers
  srand(time(NULL));
  for (int i= 0; i < 1000000; i++)
  {
    longlong n= ((longlong)rand()) | (((longlong)rand()) << 31);
    if (rand() & 16384)
      n= -n;
    test_signed(Compact_coder::get_signed_encoded_length(n), n);
  }
}


#endif
