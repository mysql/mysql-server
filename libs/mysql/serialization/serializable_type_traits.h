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

#ifndef MYSQL_SERIALIZATION_SERIALIZABLE_TYPE_TRAITS_H
#define MYSQL_SERIALIZATION_SERIALIZABLE_TYPE_TRAITS_H

#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mysql/serialization/serializable.h"
#include "mysql/utils/is_specialization.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief This function checks whether given type is enum type
/// @tparam T tested type
/// @return Answer to question: is enum type ?
template <class T>
static constexpr bool is_enum_type() {
  return std::is_enum<std::decay_t<T>>::value;
}

/// @brief This function checks whether given type is serializable
/// @tparam T tested type
/// @return Answer to question: is serializable ?
template <class T>
static constexpr bool is_serializable_type() {
  return std::is_base_of<Serializable<std::decay_t<T>>, std::decay_t<T>>::value;
}

/// @brief This function checks whether given type is STL list or vector
/// @tparam T tested type
/// @return Answer to question: is list or vector ?
template <class T>
static constexpr bool is_vector_list_type() {
  return utils::Is_specialization<std::decay_t<T>, std::vector>::value ||
         utils::Is_specialization<std::decay_t<T>, std::list>::value;
}

/// @brief This function checks whether given type is STL map or unordered map
/// @tparam T tested type
/// @return Answer to question: is map or unordered_map ?
template <class T>
static constexpr bool is_map_type() {
  return utils::Is_specialization<std::decay_t<T>, std::map>::value ||
         utils::Is_specialization<std::decay_t<T>, std::unordered_map>::value;
}

template <class T>
struct is_std_array : std::false_type {};

template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

/// @brief This function checks whether given type is STL array
/// @tparam T tested type
/// @return Answer to question: is STL array ?
template <class T>
static constexpr bool is_array_type_v() {
  return is_std_array<std::decay_t<T>>::value ||
         std::is_array<std::remove_const_t<T>>::value;
}

/// @brief This function checks whether given type is STL set or unordered set
/// @tparam T tested type
/// @return Answer to question: is set or unordered_set ?
template <class T>
static constexpr bool is_set_type() {
  return utils::Is_specialization<std::decay_t<T>, std::set>::value ||
         utils::Is_specialization<std::decay_t<T>, std::unordered_set>::value;
}

/// @brief This function checks whether given type is simple serializable type
/// @tparam T tested type
/// @return Answer to question: is simple serializable type?
template <typename T>
static constexpr bool is_simple_type() {
  return (is_vector_list_type<T>() == false && is_map_type<T>() == false &&
          is_set_type<T>() == false && is_serializable_type<T>() == false &&
          is_enum_type<T>() == false && is_array_type_v<T>() == false);
}

/// @brief This function checks whether given type (set) is bounded
/// @tparam T tested type
/// @return Answer to question: is bounded size type?
template <typename T>
static constexpr bool is_bounded_size_type() {
  return (is_vector_list_type<T>() == false && is_map_type<T>() == false &&
          is_set_type<T>() == false && is_serializable_type<T>() == false &&
          is_enum_type<T>() == false);
}

/// @brief This function checks whether given type is std::string
/// @tparam T tested type
/// @return Answer to question: is std::string ?
template <class T>
static constexpr bool is_string_type() {
  return std::is_same<std::decay_t<T>, std::string>::value;
}

/// @brief This function checks whether given type is integer type
/// @tparam T tested type
/// @return Answer to question: is integer type ?
template <class T>
static constexpr bool is_integral_type() {
  return std::is_integral<std::decay_t<T>>::value;
}

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_SERIALIZABLE_TYPE_TRAITS_H
