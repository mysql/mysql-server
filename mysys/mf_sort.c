/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* Sort of string pointers in string-order with radix or qsort */

#include "mysys_priv.h"
#include <m_string.h>

void my_string_ptr_sort(uchar *base, uint items, size_t size)
{
#if INT_MAX > 65536L
  uchar **ptr=0;

  if (radixsort_is_appliccable(items, size) &&
      (ptr= (uchar**) my_malloc(items*sizeof(char*),MYF(0))))
  {
    radixsort_for_str_ptr((uchar**) base,items,size,ptr);
    my_free(ptr);
  }
  else
#endif
  {
    if (size && items)
    {
      my_qsort2(base,items, sizeof(uchar*), get_ptr_compare(size),
                (void*) &size);
    }
  }
}
