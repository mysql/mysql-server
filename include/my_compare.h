/* Copyright (c) Monty Program Ab; 1991-2011
   Copyright (C) 2002-2006 MySQL AB
   Copyright (c) 2011, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _my_compare_h
#define _my_compare_h

#include "myisampack.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct st_HA_KEYSEG		/* Key-portion */
{
  CHARSET_INFO *charset;
  uint32 start;				/* Start of key in record */
  uint32 null_pos;			/* position to NULL indicator */
  uint16 bit_pos;                       /* Position to bit part */
  uint16 flag;
  uint16 length;			/* Keylength */
  uint8  type;				/* Type of key (for sort) */
  uint8  language;
  uint8  null_bit;			/* bitmask to test for NULL */
  uint8  bit_start,bit_end;		/* if bit field */
  uint8  bit_length;                    /* Length of bit part */
} HA_KEYSEG;

#define get_key_length(length,key) \
{ if (*(const uchar*) (key) != 255) \
    length= (uint) *(const uchar*) ((key)++); \
  else \
  { length= mi_uint2korr((key)+1); (key)+=3; } \
}

#define get_key_length_rdonly(length,key) \
{ if (*(const uchar*) (key) != 255) \
    length= ((uint) *(const uchar*) ((key))); \
  else \
  { length= mi_uint2korr((key)+1); } \
}

#define get_key_pack_length(length,length_pack,key) \
{ if (*(const uchar*) (key) != 255) \
  { length= (uint) *(const uchar*) ((key)++); length_pack= 1; }\
  else \
  { length=mi_uint2korr((key)+1); (key)+= 3; length_pack= 3; } \
}

#define store_key_length_inc(key,length) \
{ if ((length) < 255) \
  { *(key)++= (length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); (key)+=3; } \
}

#define size_to_store_key_length(length) ((length) < 255 ? 1 : 3)

#define get_rec_bits(bit_ptr, bit_ofs, bit_len) \
  (((((uint16) (bit_ptr)[1] << 8) | (uint16) (bit_ptr)[0]) >> (bit_ofs)) & \
   ((1 << (bit_len)) - 1))

#define set_rec_bits(bits, bit_ptr, bit_ofs, bit_len) \
{ \
  (bit_ptr)[0]= ((bit_ptr)[0] & ~(((1 << (bit_len)) - 1) << (bit_ofs))) | \
                ((bits) << (bit_ofs)); \
  if ((bit_ofs) + (bit_len) > 8) \
    (bit_ptr)[1]= ((bit_ptr)[1] & ~((1 << ((bit_len) - 8 + (bit_ofs))) - 1)) | \
                  ((bits) >> (8 - (bit_ofs))); \
}

#define clr_rec_bits(bit_ptr, bit_ofs, bit_len) \
  set_rec_bits(0, bit_ptr, bit_ofs, bit_len)

extern int ha_compare_text(CHARSET_INFO *, const uchar *, uint,
                           const uchar *, uint , my_bool, my_bool);
extern int ha_key_cmp(register HA_KEYSEG *keyseg, register const uchar *a,
		      register const uchar *b, uint key_length,
                      uint32 nextflag, uint *diff_pos);
extern HA_KEYSEG *ha_find_null(HA_KEYSEG *keyseg, const uchar *a);

/**
  Return values of index_cond_func_xxx functions.

  0=ICP_NO_MATCH  - index tuple doesn't satisfy the pushed index condition (the
                    engine should discard the tuple and go to the next one)
  1=ICP_MATCH     - index tuple satisfies the pushed index condition (the
                    engine should fetch and return the record)
  2=ICP_OUT_OF_RANGE - index tuple is out range that we're scanning, e.g. this
                      if we're scanning "t.key BETWEEN 10 AND 20" and got a
                      "t.key=21" tuple (the engine should stop scanning and
                      return HA_ERR_END_OF_FILE right away).
 -1= ICP_ERROR    - Reserved for internal errors in engines. Should not be
                    returned by index_cond_func_xxx
*/

typedef enum icp_result {
  ICP_ERROR=-1,
  ICP_NO_MATCH=0,
  ICP_MATCH=1,
  ICP_OUT_OF_RANGE=2
} ICP_RESULT;


#ifdef	__cplusplus
}
#endif
#endif /* _my_compare_h */
