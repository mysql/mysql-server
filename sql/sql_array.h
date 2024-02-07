#ifndef SQL_ARRAY_INCLUDED
#define SQL_ARRAY_INCLUDED

/* Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <array>

#include "my_alloc.h"

/**
   A wrapper class which provides array bounds checking.
   We do *not* own the array, we simply have a pointer to the first element,
   and a length.

   @remark
   We want the compiler-generated versions of:
   - the copy CTOR (memberwise initialization)
   - the assignment operator (memberwise assignment)

   This is roughly analogous to C++20's std::span.

   @tparam Element_type The type of the elements of the container.
 */
template <typename Element_type>
class Bounds_checked_array {
 public:
  // Convenience typedef, same typedef name as std::vector
  typedef Element_type value_type;

  Bounds_checked_array() : m_array(nullptr), m_size(0) {}

  Bounds_checked_array(Element_type *el, size_t size_arg)
      : m_array(el), m_size(size_arg) {}

  // NOTE: non-const reference intentional; mirrors std::span's constructor.
  template <class T, size_t N>
  explicit Bounds_checked_array(std::array<T, N> &arr)
      : m_array(arr.data()), m_size(arr.size()) {}

  // Not a constructor because it does something else from the other
  // constructors (allocates new memory instead of wrapping existing memory),
  // and also because nullptr for the first argument be ambiguous. The latter
  // could be solved with an explicit nullptr_t overload, though.
  //
  // The elements of the array are initialized with value initialization. For
  // primitive types, like int and pointers, this means the elements will be set
  // to the equivalent of 0 (or false or nullptr).
  static Bounds_checked_array Alloc(MEM_ROOT *mem_root, size_t size) {
    return {mem_root->ArrayAlloc<Element_type>(size), size};
  }

  /// Make a copy of '*this'. Allocate memory for m_array on 'mem_root'.
  Bounds_checked_array Clone(MEM_ROOT *mem_root) const {
    if (data() == nullptr) {
      return {};
    } else {
      Bounds_checked_array duplicate = Alloc(mem_root, size());
      if (duplicate.m_array != nullptr) {
        std::copy(cbegin(), cend(), duplicate.begin());
      }
      return duplicate;
    }
  }

  void reset() {
    m_array = nullptr;
    m_size = 0;
  }

  void reset(Element_type *array, size_t size) {
    m_array = array;
    m_size = size;
  }

  /**
    Set a new bound on the array. Does not resize the underlying
    array, so the new size must be smaller than or equal to the
    current size.
   */
  void resize(size_t new_size) {
    assert(new_size <= m_size);
    m_size = new_size;
  }

  /**
    Like resize(), but returns a new view of the array without modifying
    this one.
   */
  Bounds_checked_array prefix(size_t new_size) {
    assert(new_size <= m_size);
    return Bounds_checked_array(m_array, new_size);
  }

  Element_type *data() { return m_array; }

  const Element_type *data() const { return m_array; }

  Element_type &operator[](size_t n) {
    assert(n < m_size);
    return m_array[n];
  }

  const Element_type &operator[](size_t n) const {
    assert(n < m_size);
    return m_array[n];
  }

  typedef Element_type *iterator;
  typedef const Element_type *const_iterator;

  /// begin : Returns a pointer to the first element in the array.
  iterator begin() { return m_array; }
  /// end   : Returns a pointer to the past-the-end element in the array.
  iterator end() { return m_array + size(); }

  /// begin : Returns a pointer to the first element in the array.
  const_iterator begin() const { return m_array; }
  /// end   : Returns a pointer to the past-the-end element in the array.
  const_iterator end() const { return m_array + size(); }

  /// Returns a pointer to the first element in the array.
  const_iterator cbegin() const { return m_array; }
  /// Returns a pointer to the past-the-end element in the array.
  const_iterator cend() const { return m_array + size(); }

  Bounds_checked_array without_back() const {
    assert(m_size > 0);
    return Bounds_checked_array{m_array, m_size - 1};
  }

  size_t element_size() const { return sizeof(Element_type); }
  size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }

  bool is_null() const { return m_array == nullptr; }

  void pop_front() {
    assert(m_size > 0);
    m_array += 1;
    m_size -= 1;
  }

  Element_type *array() const { return m_array; }

  bool operator==(const Bounds_checked_array<Element_type> &rhs) const {
    return m_array == rhs.m_array && m_size == rhs.m_size;
  }
  bool operator!=(const Bounds_checked_array<Element_type> &rhs) const {
    return m_array != rhs.m_array || m_size != rhs.m_size;
  }

 private:
  Element_type *m_array;
  size_t m_size;
};

template <typename Element_type>
Bounds_checked_array<Element_type> make_array(Element_type *p, size_t n) {
  return Bounds_checked_array<Element_type>(p, n);
}

#endif /* SQL_ARRAY_INCLUDED */
