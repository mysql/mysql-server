/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/*
  strmov(dst, src) moves all the  characters  of  src  (including  the
  closing NUL) to dst, and returns a pointer to the new closing NUL in
  dst.	 The similar UNIX routine strcpy returns the old value of dst,
  which I have never found useful.  strmov(strmov(dst,a),b) moves a//b
  into dst, which seems useful.
*/

#include <my_global.h>
#include "m_string.h"

#ifdef strmov
#undef strmov
#define strmov strmov_overlapp
#endif

char *strmov(char *dst, const char *src)
{
  while ((*dst++ = *src++)) ;
  return dst-1;
}

