// Copyright (c) 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_ABI_HELPERS_ARRAY_VIEW_H
#define MYSQL_ABI_HELPERS_ARRAY_VIEW_H

/// @file
/// Experimental API header

#include <cstdlib>                                // std::size_t
#include <type_traits>                            // std::is_trivial_v
#include "mysql/abi_helpers/detail/array_base.h"  // Array_base
#ifdef MYSQL_SERVER
#include "my_sys.h"                     // MY_WME
#include "mysql/psi/psi_memory.h"       // PSI_memory_key
#include "mysql/service_mysql_alloc.h"  // my_malloc
#endif

/// @addtogroup GroupLibsMysqlAbiHelpers
/// @{

namespace mysql::abi_helpers {

/// Ownership-agnostic array class, which is both _trivial_ and
/// _standard-layout_.
///
/// This holds a length and a raw pointer to an array. The user has to manage
/// ownership of the memory as needed.
///
/// @tparam Element_tp The type of elements in the array.
template <class Element_tp>
  requires requires { std::is_trivial_v<Element_tp>; }
class Array_view : public detail::Array_base<Element_tp, Element_tp *> {
 private:
  using Self_t = Array_view<Element_tp>;
  using Base_t = detail::Array_base<Element_tp, Element_tp *>;

 public:
  using Element_t = Element_tp;
  Array_view() = default;

  /// Construct a view over the given array
  Array_view(Element_t *array, std::size_t size)
      : Base_t::m_size(size), Base_t::m_data(array) {}

  void assign(Element_t *array, std::size_t size) {
    this->m_data = array;
    this->m_size = (int32_t)size;
  }

#ifdef MYSQL_SERVER
  /// Create a new array of the given type, replacing the existing one without
  /// deallocating it.
  ///
  /// @note This, and @c free below, are *only* enabled in the MySQL server.
  /// This ensures that a component does not try to free memory allocated by the
  /// server or vice versa, which is disallowed on some platforms.
  ///
  /// @param size The number of elements.
  ///
  /// @param key The instrumentation key to track the allocation.
  void allocate(std::size_t size, PSI_memory_key key) {
    assign((Element_t *)my_malloc(key, sizeof(Element_t) * size, MYF(MY_WME)),
           size);
  }

  /// Free the array, assuming it was previously allocated using @c allocate (or
  /// `my_malloc`).
  ///
  /// This, and @c allocate above, are *only* enabled in the MySQL server. See
  /// above for justification.
  void free() {
    my_free(this->data());
    clear();
  }
#endif

  void clear() {
    this->m_data = nullptr;
    this->m_size = 0;
  }
};

static_assert(std::is_trivial_v<Array_view<int>>);
static_assert(std::is_trivially_default_constructible_v<Array_view<int>>);
static_assert(std::is_trivially_destructible_v<Array_view<int>>);
static_assert(std::is_trivially_copy_constructible_v<Array_view<int>>);
static_assert(std::is_trivially_move_constructible_v<Array_view<int>>);
static_assert(std::is_trivially_copy_assignable_v<Array_view<int>>);
static_assert(std::is_trivially_move_assignable_v<Array_view<int>>);
static_assert(std::is_standard_layout_v<Array_view<int>>);

}  // namespace mysql::abi_helpers

// addtogroup GroupLibsMysqlAbiHelpers
/// @}

#endif  // ifndef MYSQL_ABI_HELPERS_ARRAY_VIEW_H
