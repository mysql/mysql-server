/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Handling of uchar arrays as large bitmaps.
  We assume that the size of the used bitmap is less than ~(uint) 0

  TODO:
  Make assembler THREAD safe versions of these using test-and-set instructions
*/

#include "mysys_priv.h"
#include <my_bitmap.h>
#include <assert.h>

pthread_mutex_t LOCK_bitmap;

my_bool bitmap_init(BITMAP *map, uint bitmap_size)
{
  if (!(map->bitmap=(uchar*) my_malloc((bitmap_size+7)/8,MYF(MY_WME))))
    return 1;
  dbug_assert(bitmap_size != ~(uint) 0);
#ifdef THREAD
  pthread_mutex_init(&map->mutex, NULL);
#endif
  map->bitmap_size=bitmap_size;
  return 0;
}

void bitmap_free(BITMAP *map)
{
  if (map->bitmap)
  {
    my_free((char*) map->bitmap, MYF(0));
    map->bitmap=0;
#ifdef THREAD
    pthread_mutex_destroy(&map->mutex);
#endif
  }
}

void bitmap_set_bit(BITMAP *map, uint bitmap_bit)
{
  if (bitmap_bit < map->bitmap_size)
  {
    pthread_mutex_lock(&map->mutex);
    map->bitmap[bitmap_bit / 8] |= (1 << (bitmap_bit & 7));
    pthread_mutex_unlock(&map->mutex);
  }
}


uint bitmap_set_next(BITMAP *map)
{
  uchar *bitmap=map->bitmap;
  uint bit_found = MY_BIT_NONE;
  uint bitmap_size=map->bitmap_size;
  uint i;

  pthread_mutex_lock(&map->mutex);
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
  pthread_mutex_unlock(&map->mutex);
  return bit_found;
}


void bitmap_clear_bit(BITMAP *map, uint bitmap_bit)
{
  if (bitmap_bit < map->bitmap_size)
  {
    pthread_mutex_lock(&map->mutex);
    map->bitmap[bitmap_bit / 8] &= ~ (1 << (bitmap_bit & 7));
    pthread_mutex_unlock(&map->mutex);
  }
}

