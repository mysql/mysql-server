/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MEMROOT_ALLOCATOR_INCLUDED
#define MEMROOT_ALLOCATOR_INCLUDED

#include <limits>
#include <new>
#include <utility>  // std::forward

#include "my_alloc.h"
#include "my_dbug.h"

/**
  Memroot_allocator is a C++ STL memory allocator based on MEM_ROOT.

  No deallocation is done by this allocator. Calling init_sql_alloc()
  and free_root() on the supplied MEM_ROOT is the responsibility of
  the caller. Do *not* call free_root() until the destructor of any
  objects using this allocator has completed. This includes iterators.

  Example of use:
  vector<int, Memroot_allocator<int> > v((Memroot_allocator<int>(&mem_root)));

  @note allocate() throws std::bad_alloc() similarly to the default
  STL memory allocator. This is necessary - STL functions which allocate
  memory expect it. Otherwise these functions will try to use the memory,
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

template <class T>
class Memroot_allocator {
  // This cannot be const if we want to be able to swap.
  MEM_ROOT *m_memroot;

 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef T *pointer;
  typedef const T *const_pointer;

  typedef T &reference;
  typedef const T &const_reference;

  pointer address(reference r) const { return &r; }
  const_pointer address(const_reference r) const { return &r; }

  explicit Memroot_allocator(MEM_ROOT *memroot) : m_memroot(memroot) {}

  template <class U>
  Memroot_allocator(const Memroot_allocator<U> &other)
      : m_memroot(other.memroot()) {}

  template <class U>
  Memroot_allocator &operator=(
      const Memroot_allocator<U> &other MY_ATTRIBUTE((unused))) {
    DBUG_ASSERT(m_memroot == other.memroot());  // Don't swap memroot.
  }

  ~Memroot_allocator() {}

  pointer allocate(size_type n, const_pointer hint MY_ATTRIBUTE((unused)) = 0) {
    if (n == 0) return NULL;
    if (n > max_size()) throw std::bad_alloc();

    pointer p = static_cast<pointer>(alloc_root(m_memroot, n * sizeof(T)));
    if (p == NULL) throw std::bad_alloc();
    return p;
  }

  void deallocate(pointer, size_type) {}

  template <class U, class... Args>
  void construct(U *p, Args &&... args) {
    DBUG_ASSERT(p != NULL);
    try {
      ::new ((void *)p) U(std::forward<Args>(args)...);
    } catch (...) {
      DBUG_ASSERT(false);  // Constructor should not throw an exception.
    }
  }

  void destroy(pointer p) {
    DBUG_ASSERT(p != NULL);
    try {
      p->~T();
    } catch (...) {
      DBUG_ASSERT(false);  // Destructor should not throw an exception
    }
  }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }

  template <class U>
  struct rebind {
    typedef Memroot_allocator<U> other;
  };

  MEM_ROOT *memroot() const { return m_memroot; }
};

template <class T>
bool operator==(const Memroot_allocator<T> &a1,
                const Memroot_allocator<T> &a2) {
  return a1.memroot() == a2.memroot();
}

template <class T>
bool operator!=(const Memroot_allocator<T> &a1,
                const Memroot_allocator<T> &a2) {
  return a1.memroot() != a2.memroot();
}

#endif  // MEMROOT_ALLOCATOR_INCLUDED
