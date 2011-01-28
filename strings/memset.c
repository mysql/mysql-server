/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/*  File   : memset.c
    Author : Richard A. O'Keefe.
    Updated: 25 May 1984
    Defines: memset()

    memset(dst, chr, len)
    fills the memory area dst[0..len-1] with len bytes all equal to chr.
    The result is dst.	See also bfill(), which has no return value and
    puts the last two arguments the other way around.

    Note: the VAX assembly code version can only handle 0 <= len < 2^16.
    It is presented for your interest and amusement.
*/

#include "strings.h"

#if	VaxAsm

char *memset(char *dst,int chr, int len)
{
  asm("movc5 $0,*4(ap),8(ap),12(ap),*4(ap)");
  return dst;
}

#else  ~VaxAsm

char *memset(char *dst, register pchar chr, register int len)
{
  register char *d;

  for (d = dst; --len >= 0; *d++ = chr) ;
  return dst;
}

#endif	VaxAsm
