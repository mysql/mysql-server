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

/*  File   : bmove.c
    Author : Richard A. O'Keefe.
	     Michael Widenius;	ifdef MC68000
    Updated: 23 April 1984
    Defines: bmove()

    bmove(dst, src, len) moves exactly "len" bytes from the source "src"
    to the destination "dst".  It does not check for NUL characters as
    strncpy() and strnmov() do.  Thus if your C compiler doesn't support
    structure assignment, you can simulate it with
    bmove(&to, &from, sizeof from);
    The standard 4.2bsd routine for this purpose is bcopy.  But as bcopy
    has its first two arguments the other way around you may find this a
    bit easier to get right.
    No value is returned.
*/

#include <my_global.h>
#include "m_string.h"

#if !defined(HAVE_BMOVE) && !defined(bmove)

void bmove(dst, src, len)
register char *dst;
register const char *src;
register uint len;
{
  while (len-- != 0) *dst++ = *src++;
}

#endif
