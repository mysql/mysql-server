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

/*  File   : strfill.c
    Author : Monty
    Updated: 1987.04.16
    Defines: strfill()

    strfill(dest, len, fill) makes a string of fill-characters. The result
    string is of length == len. The des+len character is allways set to NULL.
    strfill() returns pointer to dest+len;
*/

#include <my_global.h>
#include "m_string.h"

char * strfill(char *s, size_t len, pchar fill)
{
  while (len--) *s++ = fill;
  *(s) = '\0';
  return(s);
} /* strfill */
