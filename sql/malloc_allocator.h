/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MALLOC_ALLOCATOR_INCLUDED
#define MALLOC_ALLOCATOR_INCLUDED

#include "my_global.h"
#include "my_sys.h"

#include <new>
#include <limits>


/**
  Malloc_allocator is a C++ STL memory allocator based on my_malloc/my_free.

  This allows for P_S instrumentation of memory allocation done by
  internally by STL container classes.

  Example usage:
  vector<int, Malloc_allocator<int> >
    v((Malloc_allocator<int>(PSI_NOT_INSTRUMENTED)));

  @note allocate() throws std::bad_alloc() similarly to the default
  STL memory allocator. This is necessary - STL functions which allocates
  memory expects it. Otherwise these functions will try to use the memory,
  leading to seg faults if memory allocation was not successful.

  @note This allocator cannot be used for std::basic_string
  because of this libstd++ bug:
  http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
  "basic_string assumes that allocators are default-constructible"
*/

template <class T> class Malloc_allocator
{
  // This cannot be const if we want to be able to swap.
  PSI_memory_key m_key;

public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef T* pointer;
  typedef const T* const_pointer;

  typedef T& reference;
  typedef const T& const_reference;

  pointer address(reference r) const { return &r; }
  const_pointer address(const_reference r) const { return &r; }

  explicit Malloc_allocator(PSI_memory_key key) : m_key(key)
  {}

  template <class U> Malloc_allocator(const Malloc_allocator<U> &other)
    : m_key(other.psi_key())
  {}

  template <class U> Malloc_allocator & operator=
    (const Malloc_allocator<U> &other)
  {
    DBUG_ASSERT(m_key == other.psi_key()); // Don't swap key.
  }

  ~Malloc_allocator()
  {}

  pointer allocate(size_type n, const_pointer hint= 0)
  {
    if (n == 0)
      return NULL;
    if (n > max_size())
      throw std::bad_alloc();

    pointer p= static_cast<pointer>(my_malloc(m_key, n * sizeof(T),
                                              MYF(MY_WME | ME_FATALERROR)));
    if (p == NULL)
      throw std::bad_alloc();
    return p;
  }

  void deallocate(pointer p, size_type n) { my_free(p); }

  void construct(pointer p, const T& val)
  {
    DBUG_ASSERT(p != NULL);
    try {
      new(p) T(val);
    } catch (...) {
      DBUG_ASSERT(false); // Constructor should not throw an exception.
    }
  }

  void destroy(pointer p)
  {
    DBUG_ASSERT(p != NULL);
    try {
      p->~T();
    } catch (...) {
      DBUG_ASSERT(false); // Destructor should not throw an exception
    }
  }

  size_type max_size() const
  {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }

  template <class U> struct rebind { typedef Malloc_allocator<U> other; };

  PSI_memory_key psi_key() const { return m_key; }
};

template <class T>
bool operator== (const Malloc_allocator<T>& a1, const Malloc_allocator<T>& a2)
{
  return a1.psi_key() == a2.psi_key();
}

template <class T>
bool operator!= (const Malloc_allocator<T>& a1, const Malloc_allocator<T>& a2)
{
  return a1.psi_key() != a2.psi_key();
}

#endif // MALLOC_ALLOCATOR_INCLUDED
