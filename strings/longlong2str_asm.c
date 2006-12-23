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

/*
  Wrapper for longlong2str.s

  We need this because the assembler code can't access the local variable
  _dig_vector in a portable manner.
*/

#include <my_global.h>
#include "m_string.h"

extern char *longlong2str_with_dig_vector(longlong val,char *dst,int radix,
                                          const char *dig_vector);

char *longlong2str(longlong val,char *dst,int radix)
{
  return longlong2str_with_dig_vector(val, dst, radix, _dig_vec_upper);
}
