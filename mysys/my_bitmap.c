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
*/

#include "mysys_priv.h"
#include <my_bitmap.h>

pthread_mutex_t LOCK_bitmap;

void bitmap_set_bit(uchar *bitmap, uint bitmap_size, uint bitmap_bit) {
  if((bitmap_bit != MY_BIT_NONE) && (bitmap_bit < bitmap_size*8)) {
    pthread_mutex_lock(&LOCK_bitmap);
    bitmap[bitmap_bit / 8] |= (1 << bitmap_bit % 8);
    pthread_mutex_unlock(&LOCK_bitmap);
  };
};

uint bitmap_set_next(uchar *bitmap, uint bitmap_size) {
  uint bit_found = MY_BIT_NONE;
  int i, b;

  pthread_mutex_lock(&LOCK_bitmap);
  for(i=0; (i<bitmap_size) && (bit_found==MY_BIT_NONE); i++) {
    if(bitmap[i] == 0xff) continue;
    for(b=0; (b<8) && (bit_found==MY_BIT_NONE); b++)
      if((bitmap[i] & 1<<b) == 0) {
        bit_found = (i*8)+b;
        bitmap[i] |= 1<<b;
      };
  };
  pthread_mutex_unlock(&LOCK_bitmap);

  return bit_found;
};

void bitmap_clear_bit(uchar *bitmap, uint bitmap_size, uint bitmap_bit) {
  if((bitmap_bit != MY_BIT_NONE) && (bitmap_bit < bitmap_size*8)) {
    pthread_mutex_lock(&LOCK_bitmap);
    bitmap[bitmap_bit / 8] &= ~(1 << bitmap_bit % 8);
    pthread_mutex_unlock(&LOCK_bitmap);
  };
};

