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

/*  File   : bchange.c
    Author : Michael widenius
    Updated: 1987-03-20
    Defines: bchange()

    bchange(dst, old_length, src, new_length, tot_length)
    replaces old_length characters at dst to new_length characters from
    src in a buffer with tot_length bytes.
*/

#include <my_global.h>
#include "m_string.h"

void bchange(register uchar *dst, size_t old_length, register const uchar *src,
	     size_t new_length, size_t tot_length)
{
  size_t rest=tot_length-old_length;
  if (old_length < new_length)
    bmove_upp(dst+rest+new_length,dst+tot_length,rest);
  else
    bmove(dst+new_length,dst+old_length,rest);
  memcpy(dst,src,new_length);
}
