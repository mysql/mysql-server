/* Copyright (c) 2000-2002, 2006 MySQL AB
   Use is subject to license terms.
   
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
   Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301, USA */

/*  File   : strxmov.c
    Author : Richard A. O'Keefe.
    Updated: 25 may 1984
    Defines: strxmov()

    strxmov(dst, src1, ..., srcn, NullS)
    moves the concatenation of src1,...,srcn to dst, terminates it
    with a NUL character, and returns a pointer to the terminating NUL.
    It is just like strmov except that it concatenates multiple sources.
    Beware: the last argument should be the null character pointer.
    Take VERY great care not to omit it!  Also be careful to use NullS
    and NOT to use 0, as on some machines 0 is not the same size as a
    character pointer, or not the same bit pattern as NullS.
*/

#include <my_global.h>
#include "m_string.h"
#include <stdarg.h>

char *strxmov(char *dst,const char *src, ...)
{
  va_list pvar;

  va_start(pvar,src);
  while (src != NullS) {
    while ((*dst++ = *src++)) ;
    dst--;
    src = va_arg(pvar, char *);
  }
  va_end(pvar);
  *dst = 0;			/* there might have been no sources! */
  return dst;
}
