/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Sort of string pointers in string-order with radix or qsort */

#include "mysys_priv.h"
#include <m_string.h>

void my_string_ptr_sort(void *base, uint items, size_s size)
{
#if INT_MAX > 65536L
  uchar **ptr=0;

  if (size <= 20 && items >= 1000 &&
      (ptr= (uchar**) my_malloc(items*sizeof(char*),MYF(0))))
  {
    radixsort_for_str_ptr((uchar**) base,items,size,ptr);
    my_free((gptr) ptr,MYF(0));
  }
  else
#endif
  {
    if (size && items)
    {
      uint size_arg=size;
      qsort2(base,items,sizeof(byte*),get_ptr_compare(size),(void*) &size_arg);
    }
  }
}
