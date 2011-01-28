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

/*  File   : strcmp.c
    Author : Richard A. O'Keefe.
    Updated: 10 April 1984
    Defines: strcmp()

    strcmp(s, t) returns > 0, = 0,  or < 0  when s > t, s = t,	or s < t
    according  to  the	ordinary  lexicographical  order.   To	test for
    equality, the macro streql(s,t) is clearer than  !strcmp(s,t).  Note
    that  if the string contains characters outside the range 0..127 the
    result is machine-dependent; PDP-11s and  VAXen  use  signed  bytes,
    some other machines use unsigned bytes.
*/

#include "strings.h"

int strcmp(register const char *s, register const char *t)
{
  while (*s == *t++) if (!*s++) return 0;
  return s[0]-t[-1];
}
