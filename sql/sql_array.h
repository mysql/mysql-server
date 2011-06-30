#ifndef SQL_ARRAY_INCLUDED
#define SQL_ARRAY_INCLUDED

/* Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_sys.h>

/*
  A typesafe wrapper around DYNAMIC_ARRAY
*/

template <class Elem> class Dynamic_array
{
  DYNAMIC_ARRAY  array;
public:
  Dynamic_array(uint prealloc=16, uint increment=16)
  {
    init(prealloc, increment);
  }

  void init(uint prealloc=16, uint increment=16)
  {
    my_init_dynamic_array(&array, sizeof(Elem), prealloc, increment);
  }

  Elem& at(int idx)
  {
    return *(((Elem*)array.buffer) + idx);
  }

  Elem *front()
  {
    return (Elem*)array.buffer;
  }

  Elem *back()
  {
    return ((Elem*)array.buffer) + array.elements;
  }

  bool append(Elem &el)
  {
    return (insert_dynamic(&array, &el));
  }

  int elements()
  {
    return array.elements;
  }

  void clear()
  {
    array.elements= 0;
  }

  ~Dynamic_array()
  {
    delete_dynamic(&array);
  }

  typedef int (*CMP_FUNC)(const Elem *el1, const Elem *el2);

  void sort(CMP_FUNC cmp_func)
  {
    my_qsort(array.buffer, array.elements, sizeof(Elem), (qsort_cmp)cmp_func);
  }
};

#endif /* SQL_ARRAY_INCLUDED */
