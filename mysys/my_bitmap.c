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
  We assume that the size of the used bitmap is less than ~(uint) 0

  TODO:
  Make assembler THREAD safe versions of these using test-and-set instructions
*/

#include "mysys_priv.h"
#include <my_bitmap.h>
#include <assert.h>
#include <m_string.h>

inline void bitmap_lock(MY_BITMAP *map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_lock(map->mutex);
#endif
}

inline void bitmap_unlock(MY_BITMAP *map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_unlock(map->mutex);
#endif
}

my_bool bitmap_init(MY_BITMAP *map, uchar *buf, uint bitmap_size, my_bool thread_safe)
{
  // for efficiency reasons - MY_BITMAP is heavily used
  DBUG_ASSERT((bitmap_size & 7) == 0);
  bitmap_size/=8;
  if (!(map->bitmap=buf) &&
       !(map->bitmap=(uchar*)my_malloc(bitmap_size + sizeof(pthread_mutex_t),
                                       MYF(MY_WME | MY_ZEROFILL))))
    return 1;
  map->bitmap_size=bitmap_size;
#ifdef THREAD
  if (thread_safe)
  {
    map->mutex=(pthread_mutex_t *)(map->bitmap+bitmap_size);
    pthread_mutex_init(map->mutex, MY_MUTEX_INIT_FAST);
  }
  else
    map->mutex=0;
#endif
  return 0;
}

void bitmap_free(MY_BITMAP *map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_destroy(map->mutex);
#endif
  if (map->bitmap)
  {
    my_free((char*) map->bitmap, MYF(0));
    map->bitmap=0;
  }
}

void bitmap_set_bit(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size*8);
  bitmap_lock(map);
  map->bitmap[bitmap_bit / 8] |= (1 << (bitmap_bit & 7));
  bitmap_unlock(map);
}

uint bitmap_set_next(MY_BITMAP *map)
{
  uchar *bitmap=map->bitmap;
  uint bit_found = MY_BIT_NONE;
  uint bitmap_size=map->bitmap_size*8;
  uint i;

  DBUG_ASSERT(map->bitmap);
  bitmap_lock(map);
  for (i=0; i < bitmap_size ; i++, bitmap++)
  {
    if (*bitmap != 0xff)
    {						/* Found slot with free bit */
      uint b;
      for (b=0; ; b++)
      {
	if (!(*bitmap & (1 << b)))
	{
	  *bitmap |= 1<<b;
	  bit_found = (i*8)+b;
	  break;
	}
      }
      break;					/* Found bit */
    }
  }
  bitmap_unlock(map);
  return bit_found;
}

void bitmap_clear_bit(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size*8);
  bitmap_lock(map);
  map->bitmap[bitmap_bit / 8] &= ~ (1 << (bitmap_bit & 7));
  bitmap_unlock(map);
}

void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bytes, prefix_bits;

  DBUG_ASSERT(map->bitmap);
  bitmap_lock(map);
  set_if_smaller(prefix_size, map->bitmap_size*8);
  if ((prefix_bytes= prefix_size / 8))
    memset(map->bitmap, 0xff, prefix_bytes);
  if ((prefix_bits= prefix_size & 7))
    map->bitmap[prefix_bytes++]= (1 << prefix_bits)-1;
  if (prefix_bytes < map->bitmap_size)
    bzero(map->bitmap+prefix_bytes, map->bitmap_size-prefix_bytes);
  bitmap_unlock(map);
}

void bitmap_clear_all(MY_BITMAP *map)
{
  bitmap_set_prefix(map, 0);
}

void bitmap_set_all(MY_BITMAP *map)
{
  bitmap_set_prefix(map, ~0);
}

my_bool bitmap_is_prefix(MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bits= prefix_size & 7, res= 0;
  uchar *m= map->bitmap, *end_prefix= map->bitmap+prefix_size/8,
        *end= map->bitmap+map->bitmap_size;

  DBUG_ASSERT(map->bitmap && prefix_size <= map->bitmap_size*8);

  bitmap_lock(map);
  while (m < end_prefix)
    if (*m++ != 0xff)
      goto ret;

  if (prefix_bits && *m++ != (1 << prefix_bits)-1)
    goto ret;

  while (m < end)
    if (m++ != 0)
      goto ret;

  res=1;
ret:
  bitmap_unlock(map);
  return res;
}

my_bool bitmap_is_clear_all(MY_BITMAP *map)
{
  return bitmap_is_prefix(map, 0);
}

my_bool bitmap_is_set_all(MY_BITMAP *map)
{
  return bitmap_is_prefix(map, map->bitmap_size*8);
}

my_bool bitmap_is_set(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size*8);
  return map->bitmap[bitmap_bit / 8] & (1 << (bitmap_bit & 7));
}

my_bool bitmap_is_subset(MY_BITMAP *map1, MY_BITMAP *map2)
{
  uint length, res=0;
  uchar *m1=map1->bitmap, *m2=map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);
  bitmap_lock(map1);
  bitmap_lock(map2);

  end= m1+map1->bitmap_size;

  while (m1 < end)
    if ((*m1++) & ~(*m2++))
      goto ret;

  res=1;
ret:
  bitmap_unlock(map2);
  bitmap_unlock(map1);
  return res;
}

my_bool bitmap_cmp(MY_BITMAP *map1, MY_BITMAP *map2)
{
  uint res;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);
  bitmap_lock(map1);
  bitmap_lock(map2);

  res= memcmp(map1->bitmap, map2->bitmap, map1->bitmap_size)==0;

  bitmap_unlock(map2);
  bitmap_unlock(map1);
  return res;
}

void bitmap_intersect(MY_BITMAP *map, MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  bitmap_lock(map);
  bitmap_lock(map2);

  end= to+map->bitmap_size;

  while (to < end)
    *to++ &= *from++;

  bitmap_unlock(map2);
  bitmap_unlock(map);
}

void bitmap_subtract(MY_BITMAP *map, MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  bitmap_lock(map);
  bitmap_lock(map2);

  end= to+map->bitmap_size;

  while (to < end)
    *to++ &= ~(*from++);

  bitmap_unlock(map2);
  bitmap_unlock(map);
}

void bitmap_union(MY_BITMAP *map, MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  bitmap_lock(map);
  bitmap_lock(map2);

  end= to+map->bitmap_size;

  while (to < end)
    *to++ |= *from++;

  bitmap_unlock(map2);
  bitmap_unlock(map);
}

