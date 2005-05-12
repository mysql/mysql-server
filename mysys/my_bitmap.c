/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Handling of uchar arrays as large bitmaps.

  API limitations (or, rather asserted safety assumptions,
  to encourage correct programming)

    * the size of the used bitmap is less than ~(uint) 0
    * it's a multiple of 8 (for efficiency reasons)
    * when arguments are a bitmap and a bit number, the number
      must be within bitmap size
    * bitmap_set_prefix() is an exception - one can use ~0 to set all bits
    * when both arguments are bitmaps, they must be of the same size
    * bitmap_intersect() is an exception :)
      (for for Bitmap::intersect(ulonglong map2buff))

  If THREAD is defined all bitmap operations except bitmap_init/bitmap_free
  are thread-safe.

  TODO:
  Make assembler THREAD safe versions of these using test-and-set instructions
*/

#include "mysys_priv.h"
#include <my_bitmap.h>
#include <m_string.h>

void create_last_word_mask(MY_BITMAP *map)
{
  /* Get the number of used bits (1..8) in the last byte */
  unsigned int const used= 1U + ((map->bitmap_size-1U) & 0x7U);

  /*
   * Create a mask with the upper 'unused' bits set and the lower 'used'
   * bits clear. The bits within each byte is stored in big-endian order.
   */
  unsigned char const mask= (~((1 << used) - 1)) & 255;
  unsigned int byte_no= ((no_bytes_in_map(map)-1)) & ~3U;
  map->last_word_ptr= (uint32*)&map->bitmap[byte_no];

  /*
    The first bytes are to be set to zero since they represent real  bits
    in the bitvector. The last bytes are set to 0xFF since they  represent
    bytes not used by the bitvector. Finally the last byte contains  bits
    as set by the mask above.
  */

  unsigned char *ptr= (unsigned char*)&map->last_word_mask;
  switch (no_bytes_in_map(map)&3)
  {
  case 1:
    map->last_word_mask= ~0U;
    ptr[0]= mask;
    return;
  case 2:
    map->last_word_mask= ~0U;
    ptr[0]= 0;
    ptr[1]= mask;
    return;
  case 3:
    map->last_word_mask= 0U;
    ptr[2]= mask;
    ptr[3]= 0xFFU;
    return;
  case 0:
    map->last_word_mask= 0U;
    ptr[3]= mask;
    return;
  }
}

static inline void bitmap_lock(MY_BITMAP *map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_lock(map->mutex);
#endif
}

static inline void bitmap_unlock(MY_BITMAP *map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_unlock(map->mutex);
#endif
}


my_bool bitmap_init(MY_BITMAP *map, uchar *buf, uint bitmap_size,
		    my_bool thread_safe)
{
  DBUG_ENTER("bitmap_init");
  DBUG_ASSERT(bitmap_size > 0);
  if (!(map->bitmap=buf) &&
      !(map->bitmap= (uchar*) my_malloc(bitmap_size +
					(thread_safe ?
					 sizeof(pthread_mutex_t) : 0),
					MYF(MY_WME))))
    return 1;
  map->bitmap_size=bitmap_size;
  create_last_word_mask(map);
#ifdef THREAD
  if (thread_safe)
  {
    map->mutex=(pthread_mutex_t *)(map->bitmap+bitmap_size);
    pthread_mutex_init(map->mutex, MY_MUTEX_INIT_FAST);
  }
  else
    map->mutex=0;
#endif
  bitmap_clear_all(map);
  DBUG_RETURN(0);
}


void bitmap_free(MY_BITMAP *map)
{
  DBUG_ENTER("bitmap_free");
  if (map->bitmap)
  {
#ifdef THREAD
    if (map->mutex)
      pthread_mutex_destroy(map->mutex);
#endif
    my_free((char*) map->bitmap, MYF(0));
    map->bitmap=0;
  }
  DBUG_VOID_RETURN;
}


uint bitmap_set_next(MY_BITMAP *map)
{
  uint bit_found;
  DBUG_ASSERT(map->bitmap);
  if ((bit_found= bitmap_get_first(map)) != MY_BIT_NONE)
    bitmap_set_bit(map, bit_found);
  return bit_found;
}


void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bytes, prefix_bits;

  DBUG_ASSERT(map->bitmap &&
	      (prefix_size <= map->bitmap_size || prefix_size == (uint) ~0));
  set_if_smaller(prefix_size, map->bitmap_size);
  if ((prefix_bytes= prefix_size / 8))
    memset(map->bitmap, 0xff, prefix_bytes);
  if ((prefix_bits= prefix_size & 7))
    map->bitmap[prefix_bytes++]= (1 << prefix_bits)-1;
  if (prefix_bytes < no_bytes_in_map(map))
    bzero(map->bitmap+prefix_bytes, no_bytes_in_map(map)-prefix_bytes);
  *map->last_word_ptr|= map->last_word_mask; //Set last bits
}


my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bits= prefix_size & 0x7, res;
  uchar *m= map->bitmap;
  uchar *end_prefix= map->bitmap+prefix_size/8;
  uchar *end;
  DBUG_ASSERT(map->bitmap && prefix_size <= map->bitmap_size);
  end= map->bitmap+no_bytes_in_map(map);

  while (m < end_prefix)
    if (*m++ != 0xff)
      return 0;

  *map->last_word_ptr^= map->last_word_mask; //Clear bits
  res= 0;
  if (prefix_bits && *m++ != (1 << prefix_bits)-1)
    goto ret;

  while (m < end)
    if (*m++ != 0)
      goto ret;
  res= 1;
ret:
  *map->last_word_ptr|= map->last_word_mask; //Set bits again
  return res; 
}


my_bool bitmap_is_set_all(const MY_BITMAP *map)
{
  uint32 *data_ptr= (uint32*)map->bitmap;
  uint32 *end= map->last_word_ptr;
  for (; data_ptr <= end; data_ptr++)
    if (*data_ptr != 0xFFFFFFFF)
      return FALSE;
  return TRUE;
}


my_bool bitmap_is_clear_all(const MY_BITMAP *map)
{
  uint32 *data_ptr= (uint32*)map->bitmap;
  uint32 *end;
  if (*map->last_word_ptr != map->last_word_mask)
    return FALSE;
  end= map->last_word_ptr;
  for (; data_ptr < end; data_ptr++)
    if (*data_ptr)
      return FALSE;
  return TRUE;
}


my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  uint32 *m1= (uint32*)map1->bitmap, *m2= (uint32*)map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);

  end= map1->last_word_ptr;

  while (m1 <= end)
  {
    if ((*m1++) & ~(*m2++))
      return 0;
  }
  return 1;
}


void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uint32 *to= (uint32*)map->bitmap, *from= (uint32*)map2->bitmap, *end;
  uint len= no_words_in_map(map), len2 = no_words_in_map(map2);

  DBUG_ASSERT(map->bitmap && map2->bitmap);

  end= to+min(len,len2);
  *map2->last_word_ptr^= map2->last_word_mask; //Clear last bits in map2
  while (to < end)
    *to++ &= *from++;

  if (len2 < len)
  {
    end+=len-len2;
    while (to < end)
      *to++=0;
  }
  *map2->last_word_ptr|= map2->last_word_mask; //Set last bits in map
  *map->last_word_ptr|= map->last_word_mask;   //Set last bits in map2
}


void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uint32 *to= (uint32*)map->bitmap, *from= (uint32*)map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);

  end= map->last_word_ptr;

  while (to <= end)
    *to++ &= ~(*from++);
  *map->last_word_ptr|= map->last_word_mask; //Set last bits again
}


void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uint32 *to= (uint32*)map->bitmap, *from= (uint32*)map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ |= *from++;
}


void bitmap_xor(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uint32 *to= (uint32*)map->bitmap, *from= (uint32*)map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ ^= *from++;
  *map->last_word_ptr|= map->last_word_mask; //Set last bits again
}


void bitmap_invert(MY_BITMAP *map)
{
  uint32 *to= (uint32*)map->bitmap, *end;

  DBUG_ASSERT(map->bitmap);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ ^= 0xFFFFFFFF;
  *map->last_word_ptr|= map->last_word_mask; //Set last bits again
}


uint bitmap_bits_set(const MY_BITMAP *map)
{  
  uchar *m= map->bitmap;
  uchar *end= m + no_bytes_in_map(map);
  uint res= 0;

  DBUG_ASSERT(map->bitmap);
  *map->last_word_ptr^=map->last_word_mask; //Reset last bits to zero
  while (m < end)
    res+= my_count_bits_ushort(*m++);
  *map->last_word_ptr^=map->last_word_mask; //Set last bits to one again
  return res;
}

uint bitmap_get_first_set(const MY_BITMAP *map)
{
  uchar *byte_ptr;
  uint bit_found,i,j,k;
  uint32 *data_ptr, *end= map->last_word_ptr;

  DBUG_ASSERT(map->bitmap);
  data_ptr= (uint32*)map->bitmap;
  for (i=0; data_ptr <= end; data_ptr++, i++)
  {
    if (*data_ptr)
    {
      byte_ptr= (uchar*)data_ptr;
      for (j=0; ; j++, byte_ptr++)
      {
        if (*byte_ptr)
        {
          for (k=0; ; k++)
          {
            if (*byte_ptr & (1 << k))
            {
              bit_found= (i*32) + (j*8) + k;
              if (bit_found == map->bitmap_size)
                return MY_BIT_NONE;
              return bit_found;
            }
          }
          DBUG_ASSERT(1);
        }
      }
      DBUG_ASSERT(1);
    }
  }
  return MY_BIT_NONE;
}


uint bitmap_get_first(const MY_BITMAP *map)
{
  uchar *byte_ptr;
  uint bit_found= MY_BIT_NONE, i,j,k;
  uint32 *data_ptr, *end= map->last_word_ptr;

  DBUG_ASSERT(map->bitmap);
  data_ptr= (uint32*)map->bitmap;
  for (i=0; data_ptr <= end; data_ptr++, i++)
  {
    if (*data_ptr != 0xFFFFFFFF)
    {
      byte_ptr= (uchar*)data_ptr;
      for (j=0; ; j++, byte_ptr++)
      { 
        if (*byte_ptr != 0xFF)
        {
          for (k=0; ; k++)
          {
            if (!(*byte_ptr & (1 << k)))
            {
              bit_found= (i*32) + (j*8) + k;
              if (bit_found == map->bitmap_size)
                return MY_BIT_NONE;
              return bit_found;
            }
          }
          DBUG_ASSERT(1);
        }
      }
      DBUG_ASSERT(1);
    }
  }
  return MY_BIT_NONE;
}


uint bitmap_lock_set_next(MY_BITMAP *map)
{
  uint bit_found;
  bitmap_lock(map);
  bit_found= bitmap_set_next(map);
  bitmap_unlock(map);
  return bit_found;
}


void bitmap_lock_clear_bit(MY_BITMAP *map, uint bitmap_bit)
{
  bitmap_lock(map);
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size);
  bitmap_clear_bit(map, bitmap_bit);
  bitmap_unlock(map);
}


#ifdef NOT_USED
my_bool bitmap_lock_is_prefix(const MY_BITMAP *map, uint prefix_size)
{
  my_bool res;
  bitmap_lock((MY_BITMAP *)map);
  res= bitmap_is_prefix(map, prefix_size);
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


void bitmap_lock_set_all(MY_BITMAP *map)
{
  bitmap_lock(map);
  bitmap_set_all(map);
  bitmap_unlock(map);
}


void bitmap_lock_clear_all(MY_BITMAP *map)
{
  bitmap_lock(map);
  bitmap_clear_all(map);
  bitmap_unlock(map);
}


void bitmap_lock_set_prefix(MY_BITMAP *map, uint prefix_size)
{
  bitmap_lock(map);
  bitmap_set_prefix(map, prefix_size);
  bitmap_unlock(map);
}


my_bool bitmap_lock_is_clear_all(const MY_BITMAP *map)
{
  uint res;
  bitmap_lock((MY_BITMAP *)map);
  res= bitmap_is_clear_all(map);
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


my_bool bitmap_lock_is_set_all(const MY_BITMAP *map)
{
  uint res;
  bitmap_lock((MY_BITMAP *)map);
  res= bitmap_is_set_all(map);
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


my_bool bitmap_lock_is_set(const MY_BITMAP *map, uint bitmap_bit)
{
  my_bool res;
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size);
  bitmap_lock((MY_BITMAP *)map);
  res= bitmap_is_set(map, bitmap_bit);
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


my_bool bitmap_lock_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  uint res;
  bitmap_lock((MY_BITMAP *)map1);
  bitmap_lock((MY_BITMAP *)map2);
  res= bitmap_is_subset(map1, map2);
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock((MY_BITMAP *)map1);
  return res;
}


my_bool bitmap_lock_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  uint res;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);
  bitmap_lock((MY_BITMAP *)map1);
  bitmap_lock((MY_BITMAP *)map2);
  res= bitmap_cmp(map1, map2);
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock((MY_BITMAP *)map1);
  return res;
}


void bitmap_lock_intersect(MY_BITMAP *map, const MY_BITMAP *map2)
{
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);
  bitmap_intersect(map, map2);
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}


void bitmap_lock_subtract(MY_BITMAP *map, const MY_BITMAP *map2)
{
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);
  bitmap_subtract(map, map2);
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}


void bitmap_lock_union(MY_BITMAP *map, const MY_BITMAP *map2)
{
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);
  bitmap_union(map, map2);
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}


/*
  SYNOPSIS
    bitmap_bits_set()
      map
  RETURN
    Number of set bits in the bitmap.
*/
uint bitmap_lock_bits_set(const MY_BITMAP *map)
{
  uint res;
  bitmap_lock((MY_BITMAP *)map);
  DBUG_ASSERT(map->bitmap);
  res= bitmap_bits_set(map);
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


/* 
  SYNOPSIS
    bitmap_get_first()
      map
  RETURN 
    Number of first unset bit in the bitmap or MY_BIT_NONE if all bits are set.
*/
uint bitmap_lock_get_first(const MY_BITMAP *map)
{
  uint res;
  bitmap_lock((MY_BITMAP*)map);
  res= bitmap_get_first(map);
  bitmap_unlock((MY_BITMAP*)map);
  return res;
}


uint bitmap_lock_get_first_set(const MY_BITMAP *map)
{
  uint res;
  bitmap_lock((MY_BITMAP*)map);
  res= bitmap_get_first_set(map);
  bitmap_unlock((MY_BITMAP*)map);
  return res;
}


void bitmap_lock_set_bit(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size);
  bitmap_lock(map);
  bitmap_set_bit(map, bitmap_bit);
  bitmap_unlock(map);
}


void bitmap_lock_flip_bit(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size);
  bitmap_lock(map);
  bitmap_flip_bit(map, bitmap_bit);
  bitmap_unlock(map);
}
#endif
#ifdef TEST_BITMAP
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
  return FALSE;
error1:
  printf("Error in set bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in clear bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
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
  return FALSE;
error1:
  printf("Error in flip bit 1, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in flip bit 2, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
}

bool test_operators(MY_BITMAP *map, uint bitsize)
{
  return FALSE;
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
  return FALSE;
error1:
  printf("Error in set_all, bitsize = %u", bitsize);
  return TRUE;
error2:
  printf("Error in clear_all, bitsize = %u", bitsize);
  return TRUE;
error3:
  printf("Error in bitwise set all, bitsize = %u", bitsize);
  return TRUE;
error4:
  printf("Error in bitwise clear all, bitsize = %u", bitsize);
  return TRUE;
error5:
  printf("Error in set_all through set_prefix, bitsize = %u", bitsize);
  return TRUE;
error6:
  printf("Error in clear_all through set_prefix, bitsize = %u", bitsize);
  return TRUE;
}

bool test_compare_operators(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit1, test_bit2, test_bit3,test_bit4;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  MY_BITMAP map2_obj, map3_obj;
  MY_BITMAP *map2= &map2_obj, *map3= &map3_obj;
  uint32 map2buf[1024];
  uint32 map3buf[1024];
  bitmap_init(&map2_obj, (uchar*)&map2buf, bitsize, FALSE);
  bitmap_init(&map3_obj, (uchar*)&map3buf, bitsize, FALSE);
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
  printf("intersect error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error2:
  printf("union error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error3:
  printf("xor error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error4:
  printf("subtract error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return TRUE;
error5:
  printf("invert error  bitsize=%u,size=%u", bitsize,
  test_bit1);
  return TRUE;
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
  return FALSE;
error1:
  printf("No bits set  bitsize = %u", bitsize);
  return TRUE;
error2:
  printf("Wrong count of bits set, bitsize = %u", bitsize);
  return TRUE;
}

bool test_get_first_bit(MY_BITMAP *map, uint bitsize)
{
  uint i, j, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
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
  printf("get_first_set error bitsize=%u,prefix_size=%u",bitsize,test_bit);
  return TRUE;
error2:
  printf("get_first error bitsize= %u, prefix_size= %u",bitsize,test_bit);
  return TRUE;
}

bool test_get_next_bit(MY_BITMAP *map, uint bitsize)
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
  printf("get_next error  bitsize= %u, prefix_size= %u", bitsize,test_bit);
  return TRUE;
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
  return FALSE;
error1:
  printf("prefix1 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
error2:
  printf("prefix2 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
error3:
  printf("prefix3 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return TRUE;
}


bool do_test(uint bitsize)
{
  MY_BITMAP map;
  uint32 buf[1024];
  if (bitmap_init(&map, (uchar*)buf, bitsize, FALSE))
  {
    printf("init error for bitsize %d", bitsize);
    goto error;
  }
  if (test_set_get_clear_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_flip_bit(&map,bitsize))
    goto error;
  bitmap_clear_all(&map);
  if (test_operators(&map,bitsize))
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
  if (test_prefix(&map,bitsize))
    goto error;
  return FALSE;
error:
  printf("\n");
  return TRUE;
}

int main()
{
  int i;
  for (i= 1; i < 4096; i++)
  {
    printf("Start test for bitsize=%u\n",i);
    if (do_test(i))
      return -1;
  }
  printf("OK\n");
  return 0;
}

/*
This is the way the test part was compiled after a complete tree build with
debug.

gcc -DHAVE_CONFIG_H -I. -I. -I.. -I../include -I.    -g -O -DDBUG_ON
-DSAFE_MUTEX -fno-omit-frame-pointer   -DHAVE_DARWIN_THREADS
-D_P1003_1B_VISIBLE -DSIGNAL_WITH_VIO_CLOSE -DTEST_BITMAP
-DSIGNALS_DONT_BREAK_READ -DIGNORE_SIGHUP_SIGQUIT -MT
my_bitmap.o -MD -MP -MF ".deps/my_bitmap.Tpo" -c -o my_bitmap.o my_bitmap.c

gcc -o my_bitmap my_bitmap.o -L../mysys -lmysys -L../strings -lmystrings
-L../dbug -ldbug
*/

#endif
