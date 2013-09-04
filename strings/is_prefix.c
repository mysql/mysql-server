/* Copyright (c) 2000, 2001, 2006 MySQL AB
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

/*  File   : is_prefix.c
    Author : Michael Widenius
    Defines: is_prefix()

    is_prefix(s, t) returns 1 if s starts with t.
    A empty t is allways a prefix.
*/

#include <my_global.h>
#include "m_string.h"

int is_prefix(register const char *s, register const char *t)
{
  while (*t)
    if (*s++ != *t++) return 0;
  return 1;					/* WRONG */
}
