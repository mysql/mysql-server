/* Copyright (C) 2000 MySQL AB

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

/*  File   : strmake.c
    Author : Michael Widenius
    Updated: 20 Jul 1984
    Defines: strmake()

    strmake(dst,src,length) moves length characters, or until end, of src to
    dst and appends a closing NUL to dst.
    Note that if strlen(src) >= length then dst[length] will be set to \0
    strmake() returns pointer to closing null
*/

#include <my_global.h>
#include "m_string.h"

char *strmake(register char *dst, register const char *src, size_t length)
{
#ifdef EXTRA_DEBUG
  /*
    'length' is the maximum length of the string; the buffer needs
    to be one character larger to accomodate the terminating '\0'.
    This is easy to get wrong, so we make sure we write to the
    entire length of the buffer to identify incorrect buffer-sizes.
    We only initialise the "unused" part of the buffer here, a) for
    efficiency, and b) because dst==src is allowed, so initialising
    the entire buffer would overwrite the source-string. Also, we
    write a character rather than '\0' as this makes spotting these
    problems in the results easier.
  */
  uint n= 0;
  while (n < length && src[n++]);
  memset(dst + n, (int) 'Z', length - n + 1);
#endif

  while (length--)
    if (! (*dst++ = *src++))
      return dst-1;
  *dst=0;
  return dst;
}
