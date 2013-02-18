/* Copyright (c) 2000, 2001, 2006, 2007 MySQL AB
   Use is subject to license terms.

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

/*  File   : strcont.c
    Author : Monty
    Updated: 1988.07.27
    Defines: strcont()

    strcont(str, set) if str contanies any character in the string set.
    The result is the position of the first found character in str, or NullS
    if there isn't anything found.

*/

#include <my_global.h>
#include "m_string.h"

char * strcont(const char *str, const char *set)
{
  char * start = (char *) set;

  while (*str)
  {
    while (*set)
    {
      if (*set++ == *str)
	return ((char*) str);
    }
    set=start; str++;
  }
  return (NullS);
} /* strcont */
