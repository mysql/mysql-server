#ifndef SQL_ARRAY_INCLUDED
#define SQL_ARRAY_INCLUDED

/* Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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

/**
   A wrapper class which provides array bounds checking.
   We do *not* own the array, we simply have a pointer to the first element,
   and a length.

   @remark
   We want the compiler-generated versions of:
   - the copy CTOR (memberwise initialization)
   - the assignment operator (memberwise assignment)

   @param Element_type The type of the elements of the container.
 */
template <typename Element_type> class Bounds_checked_array
{
public:
  Bounds_checked_array() : m_array(NULL), m_size(0) {}

  Bounds_checked_array(Element_type *el, size_t size)
    : m_array(el), m_size(size)
  {}

  void reset() { m_array= NULL; m_size= 0; }
 
  void reset(Element_type *array, size_t size)
  {
    m_array= array;
    m_size= size;
  }

  /**
    Set a new bound on the array. Does not resize the underlying
    array, so the new size must be smaller than or equal to the
    current size.
   */
  void resize(size_t new_size)
  {
    DBUG_ASSERT(new_size <= m_size);
    m_size= new_size;
  }

  Element_type &operator[](size_t n)
  {
    DBUG_ASSERT(n < m_size);
    return m_array[n];
  }

  const Element_type &operator[](size_t n) const
  {
    DBUG_ASSERT(n < m_size);
    return m_array[n];
  }

  size_t element_size() const { return sizeof(Element_type); }
  size_t size() const         { return m_size; }

  bool is_null() const { return m_array == NULL; }

  void pop_front()
  {
    DBUG_ASSERT(m_size > 0);
    m_array+= 1;
    m_size-= 1;
  }

  Element_type *array() const { return m_array; }

  bool operator==(const Bounds_checked_array<Element_type>&rhs) const
  {
    return m_array == rhs.m_array && m_size == rhs.m_size;
  }
  bool operator!=(const Bounds_checked_array<Element_type>&rhs) const
  {
    return m_array != rhs.m_array || m_size != rhs.m_size;
  }

private:
  Element_type *m_array;
  size_t        m_size;
};

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

  /**
     @note Though formally this could be declared "const" it would be
     misleading at it returns a non-const pointer to array's data.
  */
  Elem& at(int idx)
  {
    return *(((Elem*)array.buffer) + idx);
  }
  /// Const variant of at(), which cannot change data
  const Elem& at(int idx) const
  {
    return *(((Elem*)array.buffer) + idx);
  }

  /// @returns pointer to first element; undefined behaviour if array is empty
  Elem *front()
  {
    DBUG_ASSERT(array.elements >= 1);
    return (Elem*)array.buffer;
  }

  /// @returns pointer to first element; undefined behaviour if array is empty
  const Elem *front() const
  {
    DBUG_ASSERT(array.elements >= 1);
    return (const Elem*)array.buffer;
  }

  /// @returns pointer to last element; undefined behaviour if array is empty.
  Elem *back()
  {
    DBUG_ASSERT(array.elements >= 1);
    return ((Elem*)array.buffer) + (array.elements - 1);
  }

  /// @returns pointer to last element; undefined behaviour if array is empty.
  const Elem *back() const
  {
    DBUG_ASSERT(array.elements >= 1);
    return ((const Elem*)array.buffer) + (array.elements - 1);
  }

  /**
     @retval false ok
     @retval true  OOM, @c my_error() has been called.
  */
  bool append(const Elem &el)
  {
    return insert_dynamic(&array, &el);
  }

  /// Pops the last element. Does nothing if array is empty.
  Elem& pop()
  {
    return *((Elem*)pop_dynamic(&array));
  }

  void del(uint idx)
  {
    delete_dynamic_element(&array, idx);
  }

  int elements() const
  {
    return array.elements;
  }

  void elements(uint num_elements)
  {
    DBUG_ASSERT(num_elements <= array.max_element);
    array.elements= num_elements;
  }

  void clear()
  {
    elements(0);
  }

  void set(uint idx, const Elem &el)
  {
    set_dynamic(&array, &el, idx);
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
