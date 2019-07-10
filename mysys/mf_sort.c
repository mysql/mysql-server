/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
