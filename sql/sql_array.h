#ifndef SQL_ARRAY_INCLUDED
#define SQL_ARRAY_INCLUDED

/* Copyright (c) 2003, 2005-2007 MySQL AB, 2009 Sun Microsystems, Inc.
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

/* 
  Array of pointers to Elem that uses memory from MEM_ROOT

  MEM_ROOT has no realloc() so this is supposed to be used for cases when
  reallocations are rare.
*/

template <class Elem> class Array
{
  enum {alloc_increment = 16};
  Elem **buffer;
  uint n_elements, max_element;
public:
  Array(MEM_ROOT *mem_root, uint prealloc=16)
  {
    buffer= (Elem**)alloc_root(mem_root, prealloc * sizeof(Elem**));
    max_element = buffer? prealloc : 0;
    n_elements= 0;
  }

  Elem& at(int idx)
  {
    return *(((Elem*)buffer) + idx);
  }

  Elem **front()
  {
    return buffer;
  }

  Elem **back()
  {
    return buffer + n_elements;
  }

  bool append(MEM_ROOT *mem_root, Elem *el)
  {
    if (n_elements == max_element)
    {
      Elem **newbuf;
      if (!(newbuf= (Elem**)alloc_root(mem_root, (n_elements + alloc_increment)*
                                                  sizeof(Elem**))))
      {
        return FALSE;
      }
      memcpy(newbuf, buffer, n_elements*sizeof(Elem*));
      buffer= newbuf;
    }
    buffer[n_elements++]= el;
    return FALSE;
  }

  int elements()
  {
    return n_elements;
  }

  void clear()
  {
    n_elements= 0;
  }

  typedef int (*CMP_FUNC)(Elem * const *el1, Elem *const *el2);

  void sort(CMP_FUNC cmp_func)
  {
    my_qsort(buffer, n_elements, sizeof(Elem*), (qsort_cmp)cmp_func);
  }
};

#endif /* SQL_ARRAY_INCLUDED */
