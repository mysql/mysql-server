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

static inline void bitmap_lock(MY_BITMAP* map)
{
#ifdef THREAD
  if (map->thread_safe)
    pthread_mutex_lock(&map->mutex);
#endif
}

static inline void bitmap_unlock(MY_BITMAP* map)
{
#ifdef THREAD
  if (map->thread_safe)
    pthread_mutex_unlock(&map->mutex);
#endif
}

my_bool bitmap_init(MY_BITMAP *map, uint bitmap_size, my_bool thread_safe)
{
  if (!(map->bitmap=(uchar*) my_malloc((bitmap_size+7)/8,
				       MYF(MY_WME | MY_ZEROFILL))))
    return 1;
  DBUG_ASSERT(bitmap_size != ~(uint) 0);
#ifdef THREAD
  if ((map->thread_safe = thread_safe))
    pthread_mutex_init(&map->mutex, MY_MUTEX_INIT_FAST);
#endif
  map->bitmap_size=bitmap_size;
  return 0;
}

void bitmap_free(MY_BITMAP *map)
{
  if (map->bitmap)
  {
    my_free((char*) map->bitmap, MYF(0));
    map->bitmap=0;
#ifdef THREAD
    if (map->thread_safe)
      pthread_mutex_destroy(&map->mutex);
#endif
  }
}

void bitmap_set_bit(MY_BITMAP *map, uint bitmap_bit)
{
  if (bitmap_bit < map->bitmap_size)
  {
    bitmap_lock(map);
    map->bitmap[bitmap_bit / 8] |= (1 << (bitmap_bit & 7));
    bitmap_unlock(map);
  }
}


uint bitmap_set_next(MY_BITMAP *map)
{
  uchar *bitmap=map->bitmap;
  uint bit_found = MY_BIT_NONE;
  uint bitmap_size=map->bitmap_size;
  uint i;

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
  if (bitmap_bit < map->bitmap_size)
  {
    bitmap_lock(map);
    map->bitmap[bitmap_bit / 8] &= ~ (1 << (bitmap_bit & 7));
    bitmap_unlock(map);
  }
}


void bitmap_set_all(MY_BITMAP* map)
{
  bitmap_lock(map);
  memset(map->bitmap, 0xff, (map->bitmap_size+7)/8);
  bitmap_unlock(map);
}

my_bool bitmap_is_set(MY_BITMAP* map, uint bitmap_bit)
{
  return (bitmap_bit < map->bitmap_size) ?
    (map->bitmap[bitmap_bit / 8] & (1 << (bitmap_bit & 7))) :
    0;
}

void bitmap_clear_all(MY_BITMAP* map)
{
  bitmap_lock(map);
  bzero(map->bitmap,(map->bitmap_size+7)/8);
  bitmap_unlock(map);
}
