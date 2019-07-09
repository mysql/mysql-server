/* Copyright (c) 2000, 2001, 2006, 2007 MySQL AB
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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


void strappend(char *s, size_t len, pchar fill)
{
  char *endpos;

  endpos = s+len;
  while (*s++);
  s--;
  while (s<endpos) *(s++) = fill;
  *(endpos) = '\0';
} /* strappend */
