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

#ifndef MYSQL_SERIALIZATION_FIELD_WRAPPER_H
#define MYSQL_SERIALIZATION_FIELD_WRAPPER_H

#include "mysql/serialization/field_definition.h"
#include "mysql/serialization/serialization_types.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Wrapper for fields to hold field reference and
/// user-defined, compile-time field size
/// @note This is created in order to pass information about field size
/// to the Archive
template <typename Field_type, Field_size defined_field_size>
class Field_wrapper {
 public:
  using value_type = Field_type;
  static constexpr Field_size value_size = defined_field_size;
  using Field_ref_type = Field_type &;
  using Field_ref_wrapper_type = std::reference_wrapper<Field_type>;
  /// @brief Constructor
  /// @param field_reference Reference to field
  Field_wrapper(Field_type &field_reference) : m_ref(field_reference) {}

  /// @brief field bare reference accessor
  /// @returns Reference to Field_type object
  Field_ref_type get() { return m_ref.get(); }

 private:
  Field_ref_wrapper_type m_ref;  ///< Internal reference
};

/// @brief Wrapper for fields to hold field reference and defined by the
/// user compile time size of the field
/// @note This is created in order to pass information about field size
/// to the Archive
template <typename Field_type, Field_size defined_field_size>
class Field_wrapper<const Field_type, defined_field_size> {
 public:
  using value_type = Field_type;
  static constexpr Field_size value_size = defined_field_size;
  using Field_ref_type = const Field_type &;
  using Field_ref_wrapper_type = std::reference_wrapper<const Field_type>;
  /// @brief Constructor
  /// @param field_reference Reference to field
  Field_wrapper(Field_ref_type field_reference) : m_ref(field_reference) {}

  /// @brief field bare reference accessor
  /// @returns Reference to Field_type object
  Field_ref_type get() const { return m_ref.get(); }

 private:
  Field_ref_wrapper_type m_ref;  ///< Internal reference
};

template <typename Field_type>
auto create_varlen_field_wrapper(Field_type &field_reference) {
  return Field_wrapper<Field_type, 0>(field_reference);
}

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_FIELD_WRAPPER_H
