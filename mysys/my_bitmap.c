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

  create an unique structure for this that includes the mutex and bitmap size
  make a init function that will allocate the bitmap and init the mutex
  make an end function that will free everything
*/

#include "mysys_priv.h"
#include <my_bitmap.h>

pthread_mutex_t LOCK_bitmap;

void bitmap_set_bit(uchar *bitmap, uint bitmap_size, uint bitmap_bit)
{
  if (bitmap_bit < bitmap_size*8)
  {
    pthread_mutex_lock(&LOCK_bitmap);
    bitmap[bitmap_bit / 8] |= (1 << (bitmap_bit & 7));
    pthread_mutex_unlock(&LOCK_bitmap);
  }
}

uint bitmap_set_next(uchar *bitmap, uint bitmap_size)
{
  uint bit_found = MY_BIT_NONE;
  uint i;

  pthread_mutex_lock(&LOCK_bitmap);
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
  pthread_mutex_unlock(&LOCK_bitmap);
  return bit_found;
}


void bitmap_clear_bit(uchar *bitmap, uint bitmap_size, uint bitmap_bit)
{
  if (bitmap_bit < bitmap_size*8)
  {
    pthread_mutex_lock(&LOCK_bitmap);
    bitmap[bitmap_bit / 8] &= ~ (1 << (bitmap_bit & 7));
    pthread_mutex_unlock(&LOCK_bitmap);
  }
}

