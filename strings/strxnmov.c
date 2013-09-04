/* Copyright (c) 2000-2002, 2005-2007 MySQL AB
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

/*  File   : strxnmov.c
    Author : Richard A. O'Keefe.
    Updated: 2 June 1984
    Defines: strxnmov()

    strxnmov(dst, len, src1, ..., srcn, NullS)
    moves the first len characters of the concatenation of src1,...,srcn
    to dst and add a closing NUL character.
    It is just like strnmov except that it concatenates multiple sources.
    Beware: the last argument should be the null character pointer.
    Take VERY great care not to omit it!  Also be careful to use NullS
    and NOT to use 0, as on some machines 0 is not the same size as a
    character pointer, or not the same bit pattern as NullS.

    NOTE
      strxnmov is like strnmov in that it moves up to len
      characters; dst will be padded on the right with one '\0' character.
      if total-string-length >= length then dst[length] will be set to \0
*/

#include <my_global.h>
#include "m_string.h"
#include <stdarg.h>

char *strxnmov(char *dst, size_t len, const char *src, ...)
{
  va_list pvar;
  char *end_of_dst=dst+len;

  va_start(pvar,src);
  while (src != NullS)
  {
    do
    {
      if (dst == end_of_dst)
	goto end;
    }
    while ((*dst++ = *src++));
    dst--;
    src = va_arg(pvar, char *);
  }
end:
  *dst=0;
  va_end(pvar);
  return dst;
}
