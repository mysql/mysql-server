/* Copyright (C) 2002 MySQL AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*  File   : bfill.c
    Author : Richard A. O'Keefe.
	     Michael Widenius;	ifdef MC68000
    Updated: 23 April 1984
    Defines: bfill()

    bfill(dst, len, fill) moves "len" fill characters to "dst".
    Thus to set a buffer to 80 spaces, do bfill(buff, 80, ' ').
*/

#include <my_global.h>
#include "m_string.h"

#if !defined(bfill) && !defined(HAVE_BFILL)

void bfill(dst, len, fill)
register byte *dst;
register uint len;
register pchar fill;
{
  while (len-- != 0) *dst++ = fill;
}

#endif
