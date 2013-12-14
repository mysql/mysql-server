/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */


#ifndef MEM_ROOT_ARRAY_INCLUDED
#define MEM_ROOT_ARRAY_INCLUDED

#include <my_alloc.h>

/**
   A typesafe replacement for DYNAMIC_ARRAY.
   We use MEM_ROOT for allocating storage, rather than the C++ heap.
   The interface is chosen to be similar to std::vector.

   @remark
   Unlike DYNAMIC_ARRAY, elements are properly copied
   (rather than memcpy()d) if the underlying array needs to be expanded.

   @remark
   Depending on has_trivial_destructor, we destroy objects which are
   removed from the array (including when the array object itself is destroyed).

   @remark
   Note that MEM_ROOT has no facility for reusing free space,
   so don't use this if multiple re-expansions are likely to happen.

   @param Element_type The type of the elements of the container.
          Elements must be copyable.
   @param has_trivial_destructor If true, we don't destroy elements.
          We could have used type traits to determine this.
          __has_trivial_destructor is supported by some (but not all)
          compilers we use.
*/
template<typename Element_type, bool has_trivial_destructor>
class Mem_root_array
{
public:
  Mem_root_array(MEM_ROOT *root)
    : m_root(root), m_array(NULL), m_size(0), m_capacity(0)
  {
    DBUG_ASSERT(m_root != NULL);
  }

  ~Mem_root_array()
  {
    clear();
  }

  Element_type &at(size_t n)
  {
    DBUG_ASSERT(n < size());
    return m_array[n];
  }

  const Element_type &at(size_t n) const
  {
    DBUG_ASSERT(n < size());
    return m_array[n];
  }

  // Returns a pointer to the first element in the array.
  Element_type *begin() { return &m_array[0]; }

  // Returns a pointer to the past-the-end element in the array.
  Element_type *end() { return &m_array[size()]; }

  // Erases all of the elements. 
  void clear()
  {
    if (!empty())
      chop(0);
  }

  /*
    Chops the tail off the array, erasing all tail elements.
    @param pos Index of first element to erase.
  */
  void chop(const size_t pos)
  {
    DBUG_ASSERT(pos < m_size);
    if (!has_trivial_destructor)
    {
      for (size_t ix= pos; ix < m_size; ++ix)
      {
        Element_type *p= &m_array[ix];
        p->~Element_type();              // Destroy discarded element.
      }
    }
    m_size= pos;
  }

  /*
    Reserves space for array elements.
    Copies over existing elements, in case we are re-expanding the array.

    @param  n number of elements.
    @retval true if out-of-memory, false otherwise.
  */
  bool reserve(size_t n)
  {
    if (n <= m_capacity)
      return false;

    void *mem= alloc_root(m_root, n * element_size());
    if (!mem)
      return true;
    Element_type *array= static_cast<Element_type*>(mem);

    // Copy all the existing elements into the new array.
    for (size_t ix= 0; ix < m_size; ++ix)
    {
      Element_type *new_p= &array[ix];
      Element_type *old_p= &m_array[ix];
      new (new_p) Element_type(*old_p);         // Copy into new location.
      if (!has_trivial_destructor)
        old_p->~Element_type();                 // Destroy the old element.
    }

    // Forget the old array.
    m_array= array;
    m_capacity= n;
    return false;
  }

  /*
    Adds a new element at the end of the array, after its current last
    element. The content of this new element is initialized to a copy of
    the input argument.

    @param  element Object to copy.
    @retval true if out-of-memory, false otherwise.
  */
  bool push_back(const Element_type &element)
  {
    const size_t min_capacity= 20;
    const size_t expansion_factor= 2;
    if (0 == m_capacity && reserve(min_capacity))
      return true;
    if (m_size == m_capacity && reserve(m_capacity * expansion_factor))
      return true;
    Element_type *p= &m_array[m_size++];
    new (p) Element_type(element);
    return false;
  }

  size_t capacity()     const { return m_capacity; }
  size_t element_size() const { return sizeof(Element_type); }
  bool   empty()        const { return size() == 0; }
  size_t size()         const { return m_size; }

private:
  MEM_ROOT *const m_root;
  Element_type   *m_array;
  size_t          m_size;
  size_t          m_capacity;

  // Not (yet) implemented.
  Mem_root_array(const Mem_root_array&);
  Mem_root_array &operator=(const Mem_root_array&);
};


#endif  // MEM_ROOT_ARRAY_INCLUDED
