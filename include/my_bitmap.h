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

#ifndef _my_bitmap_h_
#define _my_bitmap_h_

#define MY_BIT_NONE ~(uint)0

#ifdef	__cplusplus
extern "C" {
#endif

  extern void bitmap_set_bit(uchar *bitmap, uint bitmap_size, uint bitmap_bit);
  extern uint bitmap_set_next(uchar *bitmap, uint bitmap_size);
  extern void bitmap_clear_bit(uchar *bitmap,uint bitmap_size,uint bitmap_bit);

#ifdef	__cplusplus
}
#endif

#endif
