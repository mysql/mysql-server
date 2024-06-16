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

#ifndef MYSQL_SERIALIZATION_FIELD_DEFINITION_HELPERS_H
#define MYSQL_SERIALIZATION_FIELD_DEFINITION_HELPERS_H

#include "mysql/serialization/field_definition.h"
#include "mysql/serialization/field_functor.h"
#include "mysql/serialization/serializable.h"
#include "mysql/serialization/serialization_types.h"

/// @brief helpers aimed at deducing Field_type in case size is provided,
/// and basic field and compound field definitions

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

template <class Type>
constexpr std::size_t get_default_type_size() {
  return 0;
}

template <Field_size size, typename Field_type, typename... Args>
auto define_field_with_size(Field_type &arg, Args &&...args) {
  return Field_definition<Field_type, size>(arg, std::forward<Args>(args)...);
}

template <typename Field_type, typename... Args>
auto define_field(Field_type &arg, Args &&...args) {
  return Field_definition<Field_type, get_default_type_size<Field_type>()>(
      arg, std::forward<Args>(args)...);
}

template <typename Field_type, typename... Args>
auto define_field(const Field_type &arg, Args &&...args) {
  return Field_definition<const Field_type,
                          get_default_type_size<Field_type>()>(
      arg, std::forward<Args>(args)...);
}

template <class Serializable_derived>
auto define_compound_field(Serializable<Serializable_derived> &arg) {
  return std::ref(arg);
}

template <class Serializable_derived>
auto define_compound_field(const Serializable<Serializable_derived> &arg) {
  return std::cref(arg);
}

template <class Serializable_derived>
auto define_compound_field(const Serializable<Serializable_derived> &arg,
                           const Unknown_field_policy &policy) {
  Serializable<Serializable_derived> &n_arg =
      const_cast<Serializable<Serializable_derived> &>(arg);
  n_arg.set_unknown_field_policy(policy);
  return std::cref(arg);
}

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_FIELD_DEFINITION_HELPERS_H
