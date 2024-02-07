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

#ifndef MYSQL_SERIALIZATION_SERIALIZER_DEFAULT_H
#define MYSQL_SERIALIZATION_SERIALIZER_DEFAULT_H

#include "mysql/serialization/archive_text.h"
#include "mysql/serialization/serializable.h"
#include "mysql/serialization/serializable_type_traits.h"
#include "mysql/serialization/serializer.h"

#include "mysql/utils/enumeration_utils.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Helper tag for vector types to help compiler pick the correct method
struct Serializer_vector_list_tag {};
/// @brief Helper tag for enum types to help compiler pick the correct method
struct Serializer_enum_tag {};
/// @brief Helper tag for map types to help compiler pick the correct method
struct Serializer_map_tag {};
/// @brief Helper tag for array types to help compiler pick the correct method
struct Serializer_array_tag {};
/// @brief Helper tag for set types to help compiler pick the correct method
struct Serializer_set_tag {};
/// @brief Helper tag for serializable types to help compiler pick the correct
/// method
struct Serializer_serializable_tag {};

/// @brief Basic serializer that is implementing Serializer interface
/// @details Serializes fields and appropriate metadata to ensure that
/// messages are backward and forward compatible
/// @tparam Archive_concrete_type Type of the archive
template <class Archive_concrete_type>
class Serializer_default
    : public Serializer<Serializer_default<Archive_concrete_type>,
                        Archive_concrete_type> {
 public:
  using Serializer<Serializer_default<Archive_concrete_type>,
                   Archive_concrete_type>::Serializer;

  //   template<typename ... Args>
  //   Serializer_default(Args && ... args) :
  //   Serializer(std::forward<Args>(args)...) {}

  using Base_type = Serializer<Serializer_default<Archive_concrete_type>,
                               Archive_concrete_type>;
  using Base_type::m_archive;
  using Base_type::m_error;
  friend Base_type;

  friend Archive_concrete_type;

  /// @copydoc Serializer::encode_field
  template <class Field_type, Field_size field_size_defined>
  void encode(
      Level_type level, Field_id_type field_id,
      const Field_definition<Field_type, field_size_defined> &field_definition);

  /// @copydoc Serializer::decode_field
  template <class Field_type, Field_size field_size_defined>
  void decode(
      Level_type level, Field_id_type field_id,
      std::size_t serializable_end_pos,
      Field_definition<Field_type, field_size_defined> &field_definition);

  // internal

  /// @copydoc Serializer::get_max_size
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_bounded_size_type<Field_type>() == true>>
  static constexpr std::size_t get_max_size() {
    return Archive_concrete_type::template get_max_size<uint64_t, 0>() +
           Archive_concrete_type::template get_max_size<Field_type,
                                                        field_size_defined>();
  }

  /// @copydoc Serializer::get_max_size
  template <class Serializable_concrete_type>
  static constexpr std::size_t get_max_size() {
    auto serializable_overhead_type =
        Archive_concrete_type::template get_max_size<uint64_t, 0>();
    auto serializable_overhead_size =
        Archive_concrete_type::template get_max_size<uint64_t, 0>();
    auto serializable_overhead_last_non_ignorable_field_id =
        Archive_concrete_type::template get_max_size<uint64_t, 0>();
    return serializable_overhead_type + serializable_overhead_size +
           serializable_overhead_last_non_ignorable_field_id +
           Serializable_concrete_type::template get_max_size_internal<
               Base_type>();
  }

  /// @brief Function used internally to calculate size of
  /// fields, version for basic types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_simple_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field);

  /// @brief Function used internally to calculate size of
  /// fields, version for enumeration types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_enum_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_enum_tag = {});

  /// @brief Function used internally to calculate size of
  /// fields, version for vector
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_vector_list_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_vector_list_tag = {});

  /// @brief Function used internally to calculate size of
  /// fields, version for serializable
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_serializable_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_serializable_tag = {});

  /// @brief Function used internally to calculate size of
  /// fields, version for std::map
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_map_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_map_tag = {});

  /// @brief Function used internally to calculate size of
  /// fields, version for std::set
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_set_type<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_set_tag = {});

  /// @brief Function used internally to calculate size of
  /// fields, version for std::array
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_array_type_v<Field_type>() == true>>
  static std::size_t get_field_size(const Field_type &field,
                                    Serializer_array_tag = {});

  /// @brief Function returns size of serialized field_definition object
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] field_id Field id
  /// @param[in] field_definition Definition of the field
  template <class Field_type, Field_size field_size_defined>
  static std::size_t get_size_field_def(
      Field_id_type field_id,
      const Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function returns size of serialized field_definition object
  /// @tparam Serializable_concrete_type Type of serializable obj
  /// @param[in] field_id Field id
  /// @param[in] serializable Serializable universal reference for which size
  /// will be calculated
  /// @param[in] skip_id Skip serializable id (for repeated fields)
  template <class Serializable_concrete_type>
  static std::size_t get_size_serializable(
      Field_id_type field_id, const Serializable_concrete_type &serializable,
      bool skip_id = false);

  /// @brief This function saves serializable metadata
  /// @tparam Serializable_type serializable data type
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] skip_id Skip encoding of serializable id (for repeated fields)
  template <class Serializable_type>
  void encode_serializable_metadata(Level_type level, Field_id_type field_id,
                                    const Serializable_type &serializable,
                                    bool skip_id);

  /// @brief This function loads serializable metadata
  /// @tparam Serializable_type serializable data type
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] serializable Serializable object universal reference
  /// @param[in] skip_id Skip encoding of serializable id (for repeated fields)
  /// @returns Number of bytes decoded
  template <class Serializable_type>
  std::size_t decode_serializable_metadata(Level_type level,
                                           Field_id_type field_id,
                                           Serializable_type &serializable,
                                           bool skip_id);

  /// @brief Destructor
  virtual ~Serializer_default() = default;

 protected:
  /// @brief Function used internally to decode field_id and handle optional
  /// fields
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] field_definition Definition of the field
  /// @return Answer to question Is field provided?
  template <class Field_type, Field_size field_size_defined>
  bool decode_field_id(
      Level_type level, Field_id_type field_id,
      Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function used internally to encode field_id and handle optional
  /// fields
  /// @tparam Field_type Type of field
  /// @tparam field_size_defined Defined field size in archive
  /// @param[in] level Level of the serializable tree, may be ignored, used
  /// mainly for text formatting
  /// @param[in] field_id Field id
  /// @param[in] field_definition Definition of the field
  /// @return Answer to question Is field provided?
  template <class Field_type, Field_size field_size_defined>
  bool encode_field_id(
      Level_type level, Field_id_type field_id,
      const Field_definition<Field_type, field_size_defined> &field_definition);

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for basic types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_simple_type<Field_type>() == true>>
  void encode_field(const Field_type &field);

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for basic types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_simple_type<Field_type>() == true>>
  void decode_field(Field_type &field);

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for enumeration types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_enum_type<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_enum_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for enumeration types
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_enum_type<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_enum_tag = {});

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for vector
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_vector_list_type<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_vector_list_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for vector
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_vector_list_type<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_vector_list_tag = {});

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for serializable
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_serializable_type<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_serializable_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for serializable
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <
      class Field_type, Field_size field_size_defined,
      typename = std::enable_if_t<is_serializable_type<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_serializable_tag = {});

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for std::map
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_map_type<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_map_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for std::map
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_map_type<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_map_tag = {});

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for std::set
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_set_type<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_set_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for std::set
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_set_type<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_set_tag = {});

  /// @brief Function used internally to encode field_id and handle optional
  /// fields, version for std::array
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be saved
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_array_type_v<Field_type>() == true>>
  void encode_field(const Field_type &field, Serializer_array_tag = {});

  /// @brief Function used internally to decode field_id and handle optional
  /// fields, version for std::array
  /// @tparam Field_type Type of field
  /// @param[in] field Field that will be loaded
  template <class Field_type, Field_size field_size_defined,
            typename = std::enable_if_t<is_array_type_v<Field_type>() == true>>
  void decode_field(Field_type &field, Serializer_array_tag = {});
};

}  // namespace mysql::serialization

/// @}

#include "mysql/serialization/serializer_default_impl.hpp"

#endif  // MYSQL_SERIALIZATION_SERIALIZER_DEFAULT_H
