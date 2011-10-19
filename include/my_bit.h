/* Copyright (c) 2007, 2011, Oracle and/or its affiliates.
   Copyright (c) 2009-2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_BIT_INCLUDED
#define MY_BIT_INCLUDED

#include <my_global.h>

/*
  Some useful bit functions
*/

C_MODE_START

extern const char _my_bits_nbits[256];
extern const uchar _my_bits_reverse_table[256];

/*
  Find smallest X in 2^X >= value
  This can be used to divide a number with value by doing a shift instead
*/

static inline uint my_bit_log2(ulong value)
{
  uint bit;
  for (bit=0 ; value > 1 ; value>>=1, bit++) ;
  return bit;
}

static inline uint my_count_bits(ulonglong v)
{
#if SIZEOF_LONG_LONG > 4
  /* The following code is a bit faster on 16 bit machines than if we would
     only shift v */
  ulong v2=(ulong) (v >> 32);
  return (uint) (uchar) (_my_bits_nbits[(uchar)  v] +
                         _my_bits_nbits[(uchar) (v >> 8)] +
                         _my_bits_nbits[(uchar) (v >> 16)] +
                         _my_bits_nbits[(uchar) (v >> 24)] +
                         _my_bits_nbits[(uchar) (v2)] +
                         _my_bits_nbits[(uchar) (v2 >> 8)] +
                         _my_bits_nbits[(uchar) (v2 >> 16)] +
                         _my_bits_nbits[(uchar) (v2 >> 24)]);
#else
  return (uint) (uchar) (_my_bits_nbits[(uchar)  v] +
                         _my_bits_nbits[(uchar) (v >> 8)] +
                         _my_bits_nbits[(uchar) (v >> 16)] +
                         _my_bits_nbits[(uchar) (v >> 24)]);
#endif
}

static inline uint my_count_bits_uint32(uint32 v)
{
  return (uint) (uchar) (_my_bits_nbits[(uchar)  v] +
                         _my_bits_nbits[(uchar) (v >> 8)] +
                         _my_bits_nbits[(uchar) (v >> 16)] +
                         _my_bits_nbits[(uchar) (v >> 24)]);
}


/*
  Next highest power of two

  SYNOPSIS
    my_round_up_to_next_power()
    v		Value to check

  RETURN
    Next or equal power of 2
    Note: 0 will return 0

  NOTES
    Algorithm by Sean Anderson, according to:
    http://graphics.stanford.edu/~seander/bithacks.html
    (Orignal code public domain)

    Comments shows how this works with 01100000000000000000000000001011
*/

static inline uint32 my_round_up_to_next_power(uint32 v)
{
  v--;			/* 01100000000000000000000000001010 */
  v|= v >> 1;		/* 01110000000000000000000000001111 */
  v|= v >> 2;		/* 01111100000000000000000000001111 */
  v|= v >> 4;		/* 01111111110000000000000000001111 */
  v|= v >> 8;		/* 01111111111111111100000000001111 */
  v|= v >> 16;		/* 01111111111111111111111111111111 */
  return v+1;		/* 10000000000000000000000000000000 */
}

static inline uint32 my_clear_highest_bit(uint32 v)
{
  uint32 w=v >> 1;
  w|= w >> 1;
  w|= w >> 2;
  w|= w >> 4;
  w|= w >> 8;
  w|= w >> 16;
  return v & w;
}

static inline uint32 my_reverse_bits(uint32 key)
{
  return
    (_my_bits_reverse_table[ key      & 255] << 24) |
    (_my_bits_reverse_table[(key>> 8) & 255] << 16) |
    (_my_bits_reverse_table[(key>>16) & 255] <<  8) |
     _my_bits_reverse_table[(key>>24)      ];
}

C_MODE_END

#endif /* MY_BIT_INCLUDED */
