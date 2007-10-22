/* Copyright (C) 2003 MySQL AB

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
    return (insert_dynamic(&array, (uchar*)&el));
  }

  int elements()
  {
    return array.elements;
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

