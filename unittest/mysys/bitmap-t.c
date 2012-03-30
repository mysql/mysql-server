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

   This test was copied from the unit test inside the
   mysys/my_bitmap.c file and adapted by Mats Kindahl to use the mytap
   library.
*/

#include <my_global.h>
#include <my_sys.h>
#include <my_bitmap.h>
#include <tap.h>
#include <m_string.h>

#define MAX_TESTED_BITMAP_SIZE 1024

uint get_rand_bit(uint bitsize)
{
  return (rand() % bitsize);
}

my_bool test_set_get_clear_bit(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  printf("Error in set bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in clear bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
}

my_bool test_flip_bit(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  printf("Error in flip bit 1, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in flip bit 2, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
}

my_bool test_get_all_bits(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  diag("Error in set_all, bitsize = %u", bitsize);
  return TRUE;
error2:
  diag("Error in clear_all, bitsize = %u", bitsize);
  return TRUE;
error3:
  diag("Error in bitmap_is_set_all, bitsize = %u", bitsize);
  return TRUE;
error4:
  diag("Error in bitmap_is_clear_all, bitsize = %u", bitsize);
  return TRUE;
error5:
  diag("Error in set_all through set_prefix, bitsize = %u", bitsize);
  return TRUE;
error6:
  diag("Error in clear_all through set_prefix, bitsize = %u", bitsize);
  return TRUE;
}

my_bool test_compare_operators(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit1, test_bit2, test_bit3,test_bit4;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  MY_BITMAP map2_obj, map3_obj;
  MY_BITMAP *map2= &map2_obj, *map3= &map3_obj;
  uint32 map2buf[MAX_TESTED_BITMAP_SIZE];
  uint32 map3buf[MAX_TESTED_BITMAP_SIZE];
  bitmap_init(&map2_obj, map2buf, bitsize, FALSE);
  bitmap_init(&map3_obj, map3buf, bitsize, FALSE);
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
  return FALSE;
error1:
  diag("intersect error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error2:
  diag("union error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error3:
  diag("xor error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error4:
  diag("subtract error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error5:
  diag("invert error  bitsize=%u,size=%u", bitsize,
  test_bit1);
  return TRUE;
}

my_bool test_count_bits_set(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  diag("No bits set  bitsize = %u", bitsize);
  return TRUE;
error2:
  diag("Wrong count of bits set, bitsize = %u", bitsize);
  return TRUE;
}

my_bool test_get_first_bit(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  diag("get_first_set error bitsize=%u,prefix_size=%u",bitsize,test_bit);
  return TRUE;
error2:
  diag("get_first error bitsize= %u, prefix_size= %u",bitsize,test_bit);
  return TRUE;
}

my_bool test_get_next_bit(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  diag("get_next error  bitsize= %u, prefix_size= %u", bitsize,test_bit);
  return TRUE;
}

my_bool test_prefix(MY_BITMAP *map, uint bitsize)
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
  return FALSE;
error1:
  diag("prefix1 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
error2:
  diag("prefix2 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
error3:
  diag("prefix3 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
error4:
  diag("prefix4 error  bitsize = %u, i = %u", bitsize,i);
  return TRUE;
error5:
  diag("prefix5 error  bitsize = %u, i = %u", bitsize,i);
  return TRUE;
}

my_bool test_compare(MY_BITMAP *map, uint bitsize)
{
  MY_BITMAP map2;
  uint32 map2buf[MAX_TESTED_BITMAP_SIZE];
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  if (bitmap_init(&map2, map2buf, bitsize, FALSE))
  {
    diag("init error for bitsize %d", bitsize);
    return TRUE;
  }
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
  return FALSE;
error_is_subset:
  diag("is_subset error  bitsize = %u", bitsize);
  return TRUE;
error_is_overlapping:
  diag("is_overlapping error  bitsize = %u", bitsize);
  return TRUE;
}

my_bool test_intersect(MY_BITMAP *map, uint bitsize)
{
  uint bitsize2 = 1 + get_rand_bit(MAX_TESTED_BITMAP_SIZE - 1);
  MY_BITMAP map2;
  uint32 map2buf[MAX_TESTED_BITMAP_SIZE];
  uint i, test_bit1, test_bit2, test_bit3;
  if (bitmap_init(&map2, map2buf, bitsize2, FALSE))
  {
    diag("init error for bitsize %d", bitsize2);
    return TRUE;
  }
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
  return FALSE;
error:
  diag("intersect error  bitsize = %u, bit1 = %u, bit2 = %u, bit3 = %u",
       bitsize, test_bit1, test_bit2, test_bit3);
  return TRUE;
}

my_bool do_test(uint bitsize)
{
  MY_BITMAP map;
  uint32 buf[MAX_TESTED_BITMAP_SIZE];
  if (bitmap_init(&map, buf, bitsize, FALSE))
  {
    diag("init error for bitsize %d", bitsize);
    goto error;
  }
  if (test_set_get_clear_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_flip_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_get_all_bits(&map, bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_compare_operators(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_count_bits_set(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_get_first_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_get_next_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_prefix(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_compare(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_intersect(&map,bitsize))
    goto error;
  return FALSE;
error:
  return TRUE;
}

int main()
{
  int i;
  int const min_size = 1;
  int const max_size = MAX_TESTED_BITMAP_SIZE;
  MY_INIT("bitmap-t");

  plan(max_size - min_size);
  for (i= min_size; i < max_size; i++)
    ok(do_test(i) == 0, "bitmap size %d", i);
  return exit_status();
}
