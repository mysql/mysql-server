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

/* memcmp(lhs, rhs, len)
   compares the two memory areas lhs[0..len-1]	??  rhs[0..len-1].   It
   returns  an integer less than, equal to, or greater than 0 according
   as lhs[-] is lexicographically less than, equal to, or greater  than
   rhs[-].  Note  that this is not at all the same as bcmp, which tells
   you *where* the difference is but not what.

   Note:  suppose we have int x, y;  then memcmp(&x, &y, sizeof x) need
   not bear any relation to x-y.  This is because byte order is machine
   dependent, and also, some machines have integer representations that
   are shorter than a machine word and two equal  integers  might  have
   different  values  in the spare bits.  On a ones complement machine,
   -0 == 0, but the bit patterns are different.
*/

#include "strings.h"

#if !defined(HAVE_MEMCPY)

int memcmp(lhs, rhs, len)
     register char *lhs, *rhs;
     register int len;
{
  while (--len >= 0)
    if (*lhs++ != *rhs++) return (uchar) lhs[-1] - (uchar) rhs[-1];
  return 0;
}

#endif
