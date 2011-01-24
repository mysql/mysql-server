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

/*
  memcpy(dst, src, len)
  moves len bytes from src to dst.  The result is dst.	This is not
  the same as strncpy or strnmov, while move a maximum of len bytes
  and stop early if they hit a NUL character.  This moves len bytes
  exactly, no more, no less.  See also bcopy() and bmove() which do
  not return a value but otherwise do the same job.
*/

#include "strings.h"

char *memcpy(char *dst, register char *src, register int len)
{
  register char *d;

  for (d = dst; --len >= 0; *d++ = *src++) ;
  return dst;
}
