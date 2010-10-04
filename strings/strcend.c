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

/*  File   : strcend.c
    Author : Michael Widenius:	ifdef MC68000
    Updated: 20 April 1984
    Defines: strcend()

    strcend(s, c) returns a pointer to the  first  place  in  s where  c
    occurs,  or a pointer to the end-null of s if c does not occur in s.
*/

#include <my_global.h>
#include "m_string.h"

char *strcend(register const char *s, register pchar c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return (char*) s-1;
  }
}

