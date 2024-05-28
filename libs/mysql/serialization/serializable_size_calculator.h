// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

#ifndef MYSQL_SERIALIZATION_SERIALIZABLE_SIZE_CALCULATOR_H
#define MYSQL_SERIALIZATION_SERIALIZABLE_SIZE_CALCULATOR_H

#include <tuple>
#include <unordered_set>

#include "mysql/serialization/field_definition.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

template <typename Serializable_derived>
class Serializable;

template <typename Serializer_type, typename Serializable_type>
struct Serializable_size_calculator_helper {
  /// @brief returns max size for nested Serializable class
  /// @return max declared size
  static constexpr std::size_t get_max_size() {
    return Serializer_type::template get_max_size<Serializable_type>();
  }
};

/// @brief Helper struct used to determine Field_definition declared max size
template <typename Serializer_type, class T, Field_size S>
struct Serializable_size_calculator_helper<Serializer_type,
                                           Field_definition<T, S>> {
  /// @brief returns max size for Field_definition object
  /// @return max declared size
  static constexpr std::size_t get_max_size() {
    return Serializer_type::template get_max_size<T, S>();
  }
};

template <typename Serializer_type, typename... Args>
struct Serializable_size_calculator;

/// @brief Helper struct used to determine Serializable tuple max declared size
template <typename Serializer_type, typename... Args>
struct Serializable_size_calculator<Serializer_type, std::tuple<Args...>> {
 public:
  using value_type = std::tuple<Args...>;

  /// @brief returns tuple max declared size
  /// @return tuple max declared size
  static constexpr std::size_t get_max_size() {
    return get_max_size_helper(
        std::make_index_sequence<std::tuple_size_v<value_type>>{});
  }

 private:
  /// @brief Helper function used to sum compile time array of sizes
  /// @param a Array
  /// @param i index
  /// @return summed size
  template <typename T, std::size_t N>
  static constexpr T internal_sum_size(T const (&a)[N], std::size_t i = 0U) {
    return i < N ? (a[i] + internal_sum_size(a, i + 1U)) : T{};
  }

  /// @brief Additional level of get_max_size_helper, here we get one type from
  /// the tuple
  /// @return Max size of tuple declared by the user
  template <size_t... Is>
  static constexpr std::size_t get_max_size_helper(std::index_sequence<Is...>) {
    constexpr std::size_t arr[] = {Serializable_size_calculator_helper<
        Serializer_type, std::decay_t<decltype(std::get<Is>(
                             std::declval<value_type>()))>>::get_max_size()...};
    return internal_sum_size(arr);
  }
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_SERIALIZABLE_SIZE_CALCULATOR_H
