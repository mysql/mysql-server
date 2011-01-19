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

/*  File   : strrchr.c
    Author : Richard A. O'Keefe.
    Updated: 10 April 1984
    Defines: strrchr(), rindex()

    strrchr(s, c) returns a pointer to the  last  place  in  s	where  c
    occurs,  or  NullS if c does not occur in s. This function is called
    rindex in V7 and 4.?bsd systems; while not ideal the name is clearer
    than strrchr, so rindex  remains  in  strings.h  as  a  macro.   NB:
    strrchr  looks  for single characters, not for sets or strings.  The
    parameter 'c' is declared 'int' so it will go in a register; if your
    C compiler is happy with register char change it to that.
*/

#include "strings.h"

char *strrchr(register const char *s, register pchar c)
{
  reg3 char *t;

  t = NullS;
  do if (*s == (char) c) t = (char*) s; while (*s++);
  return (char*) t;
}
