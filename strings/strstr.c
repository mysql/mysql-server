/* Copyright (c) 2000-2002, 2004, 2006 MySQL AB
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
   MA 02110-1301, USA
*/

/*  File   : strstr.c
    Author : Monty
    Updated: 1986.11.24
    Defines: strstr()

    strstr(src, pat) looks for an instance of pat in src.  pat is not a
    regex(3) pattern, it is a literal string which must be matched exactly.
    The result is a pointer to the first character of the located instance,
    or NullS if pat does not occur in src.

*/

#include <my_global.h>
#include "m_string.h"

#ifndef HAVE_STRSTR

char *strstr(register const char *str,const char *search)
{
 register char *i,*j;
 register char first= *search;

skip:
  while (*str != '\0') {
    if (*str++ == first) {
      i=(char*) str; j=(char*) search+1;
      while (*j)
	if (*i++ != *j++) goto skip;
      return ((char*) str-1);
    }
  }
  return ((char*) 0);
} /* strstr */

#endif
