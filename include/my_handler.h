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

#ifndef _my_handler_h
#define _my_handler_h

#include "my_global.h"
#include "my_base.h"
#include "m_ctype.h"
#include "myisampack.h"

typedef struct st_HA_KEYSEG		/* Key-portion */
{
  uint8  type;				/* Type of key (for sort) */
  uint8  language;
  uint8  null_bit;			/* bitmask to test for NULL */
  uint8  bit_start,bit_end;		/* if bit field */
  uint16 flag;
  uint16 length;			/* Keylength */
  uint32 start;				/* Start of key in record */
  uint32 null_pos;			/* position to NULL indicator */
  CHARSET_INFO *charset;
} HA_KEYSEG;

#define get_key_length(length,key) \
{ if ((uchar) *(key) != 255) \
    length= (uint) (uchar) *((key)++); \
  else \
  { length=mi_uint2korr((key)+1); (key)+=3; } \
}

#define get_key_length_rdonly(length,key) \
{ if ((uchar) *(key) != 255) \
    length= ((uint) (uchar) *((key))); \
  else \
  { length=mi_uint2korr((key)+1); } \
}

#define get_key_pack_length(length,length_pack,key) \
{ if ((uchar) *(key) != 255) \
  { length= (uint) (uchar) *((key)++); length_pack=1; }\
  else \
  { length=mi_uint2korr((key)+1); (key)+=3; length_pack=3; } \
}

#define store_key_length_inc(key,length) \
{ if ((length) < 255) \
  { *(key)++=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); (key)+=3; } \
}

extern int mi_compare_text(CHARSET_INFO *, uchar *, uint, uchar *, uint ,
			   my_bool, my_bool);
extern int ha_key_cmp(register HA_KEYSEG *keyseg, register uchar *a,
		      register uchar *b, uint key_length, uint nextflag,
		      uint *diff_pos);

#endif /* _my_handler_h */
