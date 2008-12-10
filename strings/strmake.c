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
  while (length--)
  {
    if (! (*dst++ = *src++))
    {
#ifdef EXTRA_DEBUG
      /*
        'length' is the maximum length of the string; the buffer needs
        to be one character larger to accommodate the terminating
        '\0'.  This is easy to get wrong, so we make sure we write to
        the entire length of the buffer to identify incorrect
        buffer-sizes.  We only initialism the "unused" part of the
        buffer here, a) for efficiency, and b) because dst==src is
        allowed, so initializing the entire buffer would overwrite the
        source-string. Also, we write a character rather than '\0' as
        this makes spotting these problems in the results easier.

        If we are using purify/valgrind, we only set one character at
        end to be able to detect also wrong accesses after the end of
        dst.
      */
      if (length)
      {
#ifdef HAVE_purify
        dst[length-1]= 'Z';
#else
        bfill(dst, length-1, (int) 'Z');
#endif /* HAVE_purify */
      }
#endif /* EXTRA_DEBUG */
      return dst-1;
    }
  }
  *dst=0;
  return dst;
}
