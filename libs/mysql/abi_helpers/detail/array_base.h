// Copyright (c) 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_ABI_HELPERS_DETAIL_ARRAY_BASE_H
#define MYSQL_ABI_HELPERS_DETAIL_ARRAY_BASE_H

/// @file
/// Experimental API header

#include <cassert>      // assert
#include <cstddef>      // std::size_t
#include <cstdint>      // int32_t
#include <type_traits>  // is_trivial_v

/// @addtogroup GroupLibsMysqlAbiHelpers
/// @{

namespace mysql::abi_helpers::detail {

/// Base class for specific implementations of standard-layout classes for
/// arrays.
///
/// This stores the length and the array, and exposes iterators, index operators
/// and member functions to query size/emptiness.
///
/// @tparam Element_t The type of each element.
///
/// @tparam Array_t The type of the array, for example `Element_t *` or
/// `Element_t[7]`.
template <class Element_t, class Array_t>
  requires requires { std::is_trivial_v<Element_t>; }
class Array_base {
 public:
  /// @return Number of elements.
  std::size_t size() const { return m_size; }

  /// @return Number of elements (signed).
  std::ptrdiff_t ssize() const { return (std::ptrdiff_t)m_size; }

  /// @return true if there are no elements, false if there are any.
  bool empty() const { return m_size == 0; }

  /// @return true if there are any elements, false if there are none.
  operator bool() const { return m_size != 0; }

  /// Index operator (non-const).
  ///
  /// @param index Array index.
  /// @return Non-const reference to element at the given index.
  Element_t &operator[](std::size_t index) {
    assert((int32_t)index < m_size);
    return m_data[index];
  }

  /// @return Pointer to the array (non-const, equal to `begin()`).
  Element_t *data() { return m_data; }

  /// @return Iterator to the beginning (non-const).
  ///
  /// Actually, just the raw pointer. This is a contiguous iterator.
  Element_t *begin() { return m_data; }

  /// @return Iterator to the end (non-const).
  ///
  /// Actually, just the raw pointer. This is a contiguous iterator.
  Element_t *end() { return m_data + m_size; }

  /// Index operator (const).
  ///
  /// @param index Array index.
  /// @return Const reference to element at the given index.
  const Element_t &operator[](std::size_t index) const {
    assert((int32_t)index < m_size);
    return m_data[index];
  }

  /// @return Pointer to the array (const, equal to `cbegin()`).
  const Element_t *data() const { return m_data; }

  /// @return Iterator to the beginning (const).
  const Element_t *begin() const { return m_data; }

  /// @return Iterator to the end (const).
  const Element_t *end() const { return m_data + m_size; }

  /// @return Iterator to the beginning (const).
  const Element_t *cbegin() const { return m_data; }

  /// @return Iterator to the end (const).
  const Element_t *cend() const { return m_data + m_size; }

 protected:
  /// Number of elements in the array.
  ///
  /// We keep it as 32 bits, so that small arrays may fit in 64 bits.
  int32_t m_size;

  /// Array data
  Array_t m_data;
};

}  // namespace mysql::abi_helpers::detail

// addtogroup GroupLibsMysqlAbiHelpers
/// @}

#endif  // ifndef MYSQL_ABI_HELPERS_DETAIL_ARRAY_BASE_H
