/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef _my_compare_h
#define _my_compare_h

#include "myisampack.h"
#ifdef	__cplusplus
extern "C" {
#endif

#include "m_ctype.h"                            /* CHARSET_INFO */

/*
  There is a hard limit for the maximum number of keys as there are only
  8 bits in the index file header for the number of keys in a table.
  This means that 0..255 keys can exist for a table. The idea of
  HA_MAX_POSSIBLE_KEY is to ensure that one can use myisamchk & tools on
  a MyISAM table for which one has more keys than MyISAM is normally
  compiled for. If you don't have this, you will get a core dump when
  running myisamchk compiled for 128 keys on a table with 255 keys.
*/

#define HA_MAX_POSSIBLE_KEY         255         /* For myisamchk */
/*
  The following defines can be increased if necessary.
  But beware the dependency of MI_MAX_POSSIBLE_KEY_BUFF and HA_MAX_KEY_LENGTH.
*/

#define HA_MAX_KEY_LENGTH           1000        /* Max length in bytes */
#define HA_MAX_KEY_SEG              16          /* Max segments for key */

#define HA_MAX_POSSIBLE_KEY_BUFF    (HA_MAX_KEY_LENGTH + 24+ 6+6)
#define HA_MAX_KEY_BUFF  (HA_MAX_KEY_LENGTH+HA_MAX_KEY_SEG*6+8+8)

typedef struct st_HA_KEYSEG		/* Key-portion */
{
  const CHARSET_INFO *charset;
  uint32 start;				/* Start of key in record */
  uint32 null_pos;			/* position to NULL indicator */
  uint16 bit_pos;                       /* Position to bit part */
  uint16 flag;
  uint16 length;			/* Keylength */
  uint16 language;
  uint8  type;				/* Type of key (for sort) */
  uint8  null_bit;			/* bitmask to test for NULL */
  uint8  bit_start,bit_end;		/* if bit field */
  uint8  bit_length;                    /* Length of bit part */
} HA_KEYSEG;

#define get_key_length(length,key) \
{ if (*(uchar*) (key) != 255) \
    length= (uint) *(uchar*) ((key)++); \
  else \
  { length= mi_uint2korr((key)+1); (key)+=3; } \
}

#define get_key_length_rdonly(length,key) \
{ if (*(uchar*) (key) != 255) \
    length= ((uint) *(uchar*) ((key))); \
  else \
  { length= mi_uint2korr((key)+1); } \
}

#define get_key_pack_length(length,length_pack,key) \
{ if (*(uchar*) (key) != 255) \
  { length= (uint) *(uchar*) ((key)++); length_pack= 1; }\
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

extern int ha_compare_text(const CHARSET_INFO *, uchar *, uint, uchar *, uint ,
			   my_bool, my_bool);
extern int ha_key_cmp(register HA_KEYSEG *keyseg, register uchar *a,
		      register uchar *b, uint key_length, uint nextflag,
		      uint *diff_pos);

/*
  Inside an in-memory data record, memory pointers to pieces of the
  record (like BLOBs) are stored in their native byte order and in
  this amount of bytes.
*/
#define portable_sizeof_char_ptr 8


/**
  Return values of index_cond_func_xxx functions.

  0=ICP_NO_MATCH  - index tuple doesn't satisfy the pushed index condition (the
                engine should discard the tuple and go to the next one)
  1=ICP_MATCH     - index tuple satisfies the pushed index condition (the engine
                should fetch and return the record)
  2=ICP_OUT_OF_RANGE - index tuple is out range that we're scanning, e.g. this
                   if we're scanning "t.key BETWEEN 10 AND 20" and got a
                   "t.key=21" tuple (the engine should stop scanning and return
                   HA_ERR_END_OF_FILE right away).
*/

typedef enum icp_result {
  ICP_NO_MATCH,
  ICP_MATCH,
  ICP_OUT_OF_RANGE
} ICP_RESULT;


#ifdef	__cplusplus
}
#endif

#endif /* _my_compare_h */
