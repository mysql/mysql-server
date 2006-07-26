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

  TODO:
  Make assembler THREAD safe versions of these using test-and-set instructions
*/

#include "mysys_priv.h"
#include <my_bitmap.h>
#include <m_string.h>

static inline void bitmap_lock(MY_BITMAP* map)
{
#ifdef THREAD
  if (map->mutex)
    pthread_mutex_lock(map->mutex);
#endif
}

static inline void bitmap_unlock(MY_BITMAP* map)
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

  DBUG_ASSERT((bitmap_size & 7) == 0);
  bitmap_size/=8;
  if (!(map->bitmap=buf) &&
      !(map->bitmap= (uchar*) my_malloc(bitmap_size +
					(thread_safe ?
					 sizeof(pthread_mutex_t) : 0),
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


void bitmap_set_bit(MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size*8);
  bitmap_lock(map);
  bitmap_fast_set_bit(map, bitmap_bit);
  bitmap_unlock(map);
}


uint bitmap_set_next(MY_BITMAP *map)
{
  uchar *bitmap=map->bitmap;
  uint bit_found = MY_BIT_NONE;
  uint bitmap_size=map->bitmap_size;
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
  bitmap_fast_clear_bit(map, bitmap_bit);
  bitmap_unlock(map);
}


void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bytes, prefix_bits;

  DBUG_ASSERT(map->bitmap &&
	      (prefix_size <= map->bitmap_size*8 || prefix_size == (uint) ~0));
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


my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bits= prefix_size & 7, res= 0;
  uchar *m= map->bitmap, *end_prefix= map->bitmap+prefix_size/8,
        *end= map->bitmap+map->bitmap_size;

  DBUG_ASSERT(map->bitmap && prefix_size <= map->bitmap_size*8);

  bitmap_lock((MY_BITMAP *)map);
  while (m < end_prefix)
    if (*m++ != 0xff)
      goto ret;

  if (prefix_bits && *m++ != (1 << prefix_bits)-1)
    goto ret;

  while (m < end)
    if (*m++ != 0)
      goto ret;

  res=1;
ret:
  bitmap_unlock((MY_BITMAP *)map);
  return res;
}


my_bool bitmap_is_clear_all(const MY_BITMAP *map)
{
  return bitmap_is_prefix(map, 0);
}

my_bool bitmap_is_set_all(const MY_BITMAP *map)
{
  return bitmap_is_prefix(map, map->bitmap_size*8);
}


my_bool bitmap_is_set(const MY_BITMAP *map, uint bitmap_bit)
{
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->bitmap_size*8);
  return bitmap_fast_is_set(map, bitmap_bit);
}


my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  uint res=0;
  uchar *m1=map1->bitmap, *m2=map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);
  bitmap_lock((MY_BITMAP *)map1);
  bitmap_lock((MY_BITMAP *)map2);

  end= m1+map1->bitmap_size;

  while (m1 < end)
  {
    if ((*m1++) & ~(*m2++))
      goto ret;
  }

  res=1;
ret:
  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock((MY_BITMAP *)map1);
  return res;
}


my_bool bitmap_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  uint res;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->bitmap_size==map2->bitmap_size);
  bitmap_lock((MY_BITMAP *)map1);
  bitmap_lock((MY_BITMAP *)map2);

  res= memcmp(map1->bitmap, map2->bitmap, map1->bitmap_size)==0;

  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock((MY_BITMAP *)map1);
  return res;
}


void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;
  uint len=map->bitmap_size, len2=map2->bitmap_size;

  DBUG_ASSERT(map->bitmap && map2->bitmap);
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);

  end= to+min(len,len2);

  while (to < end)
    *to++ &= *from++;

  if (len2 < len)
  {
    end+=len-len2;
    while (to < end)
      *to++=0;
  }

  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}


void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);

  end= to+map->bitmap_size;

  while (to < end)
    *to++ &= ~(*from++);

  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}


void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2)
{
  uchar *to=map->bitmap, *from=map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->bitmap_size==map2->bitmap_size);
  bitmap_lock(map);
  bitmap_lock((MY_BITMAP *)map2);

  end= to+map->bitmap_size;

  while (to < end)
    *to++ |= *from++;

  bitmap_unlock((MY_BITMAP *)map2);
  bitmap_unlock(map);
}

