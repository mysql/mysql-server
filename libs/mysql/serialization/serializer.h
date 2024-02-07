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

#ifndef MYSQL_SERIALIZATION_SERIALIZER_H
#define MYSQL_SERIALIZATION_SERIALIZER_H

#include "mysql/serialization/field_definition.h"
#include "mysql/serialization/serializable.h"
#include "mysql/serialization/serialization_error.h"
#include "mysql/serialization/serialization_format_version.h"
#include "mysql/serialization/serialization_types.h"
#include "mysql/serialization/unknown_field_policy.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Interface for serializer
/// @details This class provides functionality of encoder and decoder,
/// storing the formatted data in the defined Archive_type.
/// This class converts a hierarchical message (Serializable type) into a linear
/// sequence of message boundaries and fields (with the usage of
/// Serializable interface). To do so, it traverses the message in depth-first
/// order and applies the user-defined predicates to determine if a field
/// should be included in encoded message or not. Field ids are generated
/// during iteration over fields with the usage of the Serializable interface.
/// Particular message boundaries formatting and fields formatting is decided
/// by the Serializer_derived_type.
/// The methods that Serializer_derived_type shall provide are explicitly
/// marked in methods documentation as "to be implemented by
/// Serializer_derived_type". Derived class shall provide a specific
/// formatting of message boundaries, specific formatting of particular
/// fields and size calculation methods (get_size_serializable,
/// get_size_field_def, get_max_size for simple/complex fields)
/// @tparam Serializer_derived_type Concrete Serializer type, this is CRTP
/// @tparam Archive_type Concrete archive type to store data
template <class Serializer_derived_type, class Archive_type>
class Serializer {
 public:
  using Serializer_current_type =
      Serializer<Serializer_derived_type, Archive_type>;

  bool is_error() const { return !is_good(); }
  bool is_good() const {
    return (m_error.is_error() == false && m_archive.is_good());
  }
  const Serialization_error &get_error() {
    if (m_archive.is_error()) {
      return m_archive.get_error();
    }
    return m_error;
  }

  /// @brief Function for the API user to serialize data
  /// @tparam T Serialized type, implementing Serializable interface
  /// @param[in] arg Data to be written into the archive
  /// @return Updated object for chaining
  template <typename T>
  Serializer_current_type &operator<<(const T &arg);

  /// @brief Function for the API user to serialize data
  /// @tparam T Serialized type, implementing Serializable interface
  /// @param[in] arg Deserialized data will be stored in arg
  /// @return Updated object for chaining
  template <typename T>
  Serializer_current_type &operator>>(T &arg);

  /// @brief Function for the API user to access reference of the archive
  /// @returns Reference to the archive object
  Archive_type &get_archive();

  template <typename T>
  static std::size_t get_size(const T &arg);

 protected:
  template <typename Serializable_derived_current_type>
  friend class Serializable;
  template <typename Serializer_type, typename Type>
  friend struct Serializable_size_calculator_helper;

  /// @brief Casts this to derived type
  /// @return Derived class pointer to this
  /// @note To be implemented in Archive_derived_type
  Serializer_derived_type *get_derived() {
    return static_cast<Serializer_derived_type *>(this);
  }

  /// @brief Function used to encode one field
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] field_definition Definition of the field
  /// @note To be implemented in Serializer_derived_type
  template <class Field_type, Field_size field_size_defined>
  void encode_field(
      Level_type level, Field_id_type field_id,
      const Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function used to decode one field
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Expected field id
  /// @param[in] serializable_end_pos End position of current serializable type
  /// @param[in] field_definition Definition of the field
  /// @note To be implemented in Serializer_derived_type
  template <class Field_type, Field_size field_size_defined>
  void decode_field(
      Level_type level, Field_id_type field_id,
      std::size_t serializable_end_pos,
      Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function used to encode serializable field, this function saves
  /// serializable metadata and calls serializable encode method
  /// @tparam Serializable_type serializable data type
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] skip_id Skip encoding of serializable id (for repeated fields)
  /// @note To be implemented in Serializer_derived_type
  template <class Serializable_type>
  void encode_serializable(Level_type level, Field_id_type field_id,
                           const Serializable_type &serializable, bool skip_id);

  /// @brief Function used to encode fields of specializations of Serializable
  /// class, iterates over constant fields
  /// allowing them to be saved into an archive
  /// @tparam Serializable_type serializable data type
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] level Serializable tree level, serializable may contain other
  /// serializable fields, level is used for informational purpose or
  /// additional text formatting for nested types
  /// @note To be implemented in Serializer_derived_type
  template <class Serializable_type>
  void encode_serializable_fields(const Serializable_type &serializable,
                                  Level_type level);

  /// @brief Function used to decode serializable field, this function loads
  /// serializable metadata and calls serializable decode method
  /// @tparam Serializable_type serializable data type
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] serializable_end_pos End position of current serializable type
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] skip_id Skip decoding of serializable id.
  /// In case a field is placed in a container, we skip encoding / decoding
  /// of the field id. Field id encoding / decoding is skipped in case
  /// field_id was already processed or a complex type is put in the container
  template <class Serializable_type>
  void decode_serializable(Level_type level, Field_id_type field_id,
                           std::size_t serializable_end_pos,
                           Serializable_type &serializable, bool skip_id);

  /// @brief Function used to decode fields of specializations of Serializable
  /// class, iterates over non constant fields,
  /// allowing them to be filled with data from an archive
  /// @tparam Serializable_type serializable data type
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] level Serializable tree level, serializable may contain other
  /// serializable fields, level is used for informational purpose or
  /// additional text formatting for nested types
  /// @param[in] serializable_end_pos End position of current serializable type
  template <class Serializable_type>
  void decode_serializable_fields(Serializable_type &serializable,
                                  Level_type level,
                                  std::size_t serializable_end_pos);

  /// @brief Function returns size of field object written to an
  /// archive
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] field_id Field id
  /// @param[in] field_definition Definition of the field
  /// @note To be implemented in Serializer_derived_type
  template <class Field_type, Field_size field_size_defined>
  static std::size_t get_size_field_def(
      Field_id_type field_id,
      const Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function returns size of serializable object written to an
  /// archive
  /// @tparam Serializable_concrete_type Type of serializable obj
  /// @param[in] field_id Field id
  /// @param[in] serializable Serializable universal reference for which size
  /// will be calculated
  /// @param[in] skip_id Skip serializable id (for repeated fields)
  /// @note To be implemented in Serializer_derived_type
  template <class Serializable_concrete_type>
  static std::size_t get_size_serializable(
      Field_id_type field_id, const Serializable_concrete_type &serializable,
      bool skip_id = false);

  /// @brief Function returns maximum size of the field written to an archive,
  /// based on its type
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  template <class Field_type, Field_size field_size_defined>
  static constexpr std::size_t get_max_size() {
    return Serializer_derived_type::template get_max_size<Field_type,
                                                          field_size_defined>();
  }

  /// @brief Function returns maximum size of the Serializable_concrete_type
  /// class object data written to an archive, based on its type
  /// @tparam Serializable_concrete_type Type of serializable obj
  template <class Serializable_concrete_type>
  static constexpr std::size_t get_max_size() {
    return Serializer_derived_type::template get_max_size<
        Serializable_concrete_type>();
  }

  void clear_error() {
    m_archive.clear_error();
    m_error = Serialization_error();
  }

  /// @brief Destructor
  /// @note Cannot create an object of Serializer interface
  virtual ~Serializer() = default;
  Level_type m_level{0};        ///< Level of the serializable tree
  Archive_type m_archive;       ///< Archive that stores the data
  Serialization_error m_error;  ///< Holds information about error
};

}  // namespace mysql::serialization

/// @}

#include "mysql/serialization/serializer_impl.hpp"

#endif  // MYSQL_SERIALIZATION_SERIALIZER_H
