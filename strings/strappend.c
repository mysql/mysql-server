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

/*  File   : strappend.c
    Author : Monty
    Updated: 1987.02.07
    Defines: strappend()

    strappend(dest, len, fill) appends fill-characters to a string so that
    the result length == len. If the string is longer than len it's
    trunked. The des+len character is allways set to NULL.
*/

#include <my_global.h>
#include "m_string.h"


void strappend(register char *s, size_t len, pchar fill)
{
  register char *endpos;

  endpos = s+len;
  while (*s++);
  s--;
  while (s<endpos) *(s++) = fill;
  *(endpos) = '\0';
} /* strappend */
