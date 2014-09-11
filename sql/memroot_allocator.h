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

#ifndef MEMROOT_ALLOCATOR_INCLUDED
#define MEMROOT_ALLOCATOR_INCLUDED

#include "sql_alloc.h"

#include <new>
#include <limits>


/**
  Memroot_allocator is a C++ STL memory allocator based on MEM_ROOT.

  No deallocation is done by this allocator. Calling init_sql_alloc()
  and free_root() on the supplied MEM_ROOT is the responsibility of
  the caller. Do *not* call free_root() until the destructor of any
  objects using this allocator has completed. This includes iterators.

  Example of use:
  vector<int, Memroot_allocator<int> > v((Memroot_allocator<int>(&mem_root)));

  @note allocate() throws std::bad_alloc() similarly to the default
  STL memory allocator. This is necessary - STL functions which allocates
  memory expects it. Otherwise these functions will try to use the memory,
  leading to seg faults if memory allocation was not successful.

  @note This allocator cannot be used for std::basic_string
  because of this libstd++ bug:
  http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
  "basic_string assumes that allocators are default-constructible"

  @note C++98 says that STL implementors can assume that allocator objects
  of the same type always compare equal. This will only be the case for
  two Memroot_allocators that use the same MEM_ROOT. Care should be taken
  when this is not the case. Especially:
  - Using list::splice() on two lists with allocators using two different
    MEM_ROOTs causes undefined behavior. Most implementations seem to give
    runtime errors in such cases.
  - swap() on two collections with allocators using two different MEM_ROOTs
    is not well defined. At least some implementations also swap allocators,
    but this should not be depended on.
*/

template <class T> class Memroot_allocator
{
  // This cannot be const if we want to be able to swap.
  MEM_ROOT *m_memroot;

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

  explicit Memroot_allocator(MEM_ROOT *memroot) : m_memroot(memroot)
  {}

  template <class U> Memroot_allocator(const Memroot_allocator<U> &other)
    : m_memroot(other.memroot())
  {}

  template <class U> Memroot_allocator & operator=
    (const Memroot_allocator<U> &other)
  {
    DBUG_ASSERT(m_memroot == other.memroot()); // Don't swap memroot.
  }

  ~Memroot_allocator()
  {}

  pointer allocate(size_type n, const_pointer hint= 0)
  {
    if (n == 0)
      return NULL;
    if (n > max_size())
      throw std::bad_alloc();

    pointer p= static_cast<pointer>(alloc_root(m_memroot, n * sizeof(T)));
    if (p == NULL)
      throw std::bad_alloc();
    return p;
  }

  void deallocate(pointer p, size_type n) { }

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

  template <class U> struct rebind { typedef Memroot_allocator<U> other; };

  MEM_ROOT *memroot() const { return m_memroot; }
};

template <class T>
bool operator== (const Memroot_allocator<T>& a1, const Memroot_allocator<T>& a2)
{
  return a1.memroot() == a2.memroot();
}

template <class T>
bool operator!= (const Memroot_allocator<T>& a1, const Memroot_allocator<T>& a2)
{
  return a1.memroot() != a2.memroot();
}

#endif // MEMROOT_ALLOCATOR_INCLUDED
