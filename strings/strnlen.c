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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*  File   : strnlen.c
    Author : Michael Widenius
    Updated: 20 April 1984
    Defines: strnlen.
    strnlen(s, len) returns the length of s or len if s is longer than len.
*/

#include <my_global.h>
#include "m_string.h"

#ifndef HAVE_STRNLEN

size_t strnlen(const char *s, size_t maxlen)
{
  const char *end= (const char *)memchr(s, '\0', maxlen);
  return end ? (size_t) (end - s) : maxlen;
}

#endif
