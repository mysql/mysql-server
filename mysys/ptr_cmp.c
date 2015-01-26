/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include <myisampack.h>

void my_store_ptr(uchar *buff, size_t pack_length, my_off_t pos)
{
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: mi_int8store(buff,pos); break;
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,pos); break;
  case 1: buff[0]= (uchar) pos; break;
  default: DBUG_ASSERT(0);
  }
  return;
}

my_off_t my_get_ptr(uchar *ptr, size_t pack_length)
{
  my_off_t pos;
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: pos= (my_off_t) mi_uint8korr(ptr); break;
  case 7: pos= (my_off_t) mi_uint7korr(ptr); break;
  case 6: pos= (my_off_t) mi_uint6korr(ptr); break;
  case 5: pos= (my_off_t) mi_uint5korr(ptr); break;
#endif
  case 4: pos= (my_off_t) mi_uint4korr(ptr); break;
  case 3: pos= (my_off_t) mi_uint3korr(ptr); break;
  case 2: pos= (my_off_t) mi_uint2korr(ptr); break;
  case 1: pos= (my_off_t) *(uchar*) ptr; break;
  default: DBUG_ASSERT(0); return 0;
  }
 return pos;
}

