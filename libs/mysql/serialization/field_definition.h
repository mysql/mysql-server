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

#ifndef MYSQL_SERIALIZATION_FIELD_DEFINITION_H
#define MYSQL_SERIALIZATION_FIELD_DEFINITION_H

#include <functional>

#include "mysql/serialization/field_functor.h"
#include "mysql/serialization/serializable_type_tags.h"
#include "mysql/serialization/serialization_types.h"
#include "mysql/serialization/unknown_field_policy.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Field definition provided by classes implementing Serializable
/// interface
template <class Field_type, Field_size defined_field_size = sizeof(Field_type)>
class Field_definition {
 public:
  using Tag = Field_definition_tag;
  using Field_type_ref = std::reference_wrapper<Field_type>;

  /// @brief Constructs field definition object
  /// @param[in] field Reference to described field object
  /// externally, serializer may ignore this value or use it to hide some fields
  /// @param[in] encode_predicate Function that is called for optional fields,
  /// defines whether field will be encoded
  /// @param[in] unknown_field_policy What decoder should do in case this
  /// field definition is unknown to the decoder (error / ignore)
  /// @param[in] field_missing_functor Function that is called for this
  /// field in case it is not available in encoded data
  Field_definition(Field_type &field, Field_encode_predicate encode_predicate,
                   Unknown_field_policy unknown_field_policy,
                   Field_missing_functor field_missing_functor)
      : Field_definition(field, encode_predicate, unknown_field_policy) {
    m_field_missing_functor = field_missing_functor;
  }

  /// @brief Constructs field definition object
  /// @param[in] field Reference to described field object
  /// @param[in] unknown_field_policy What decoder should do in case this
  /// field definition is unknown to the decoder (error / ignore)
  Field_definition(
      Field_type &field,
      Unknown_field_policy unknown_field_policy = Unknown_field_policy::ignore)
      : m_ref(field), m_unknown_field_policy(unknown_field_policy) {
    m_encode_predicate = Field_encode_predicate([]() -> bool { return true; });
    m_field_missing_functor = Field_missing_functor([]() -> void {});
  }

  /// @brief Constructs field definition object
  /// @param[in] field Reference to described field object
  /// @param[in] unknown_field_policy What decoder should do in case this
  /// field definition is unknown to the decoder (error / ignore)
  /// @param[in] encode_predicate Function that is called for optional fields,
  /// defines whether field will be encoded
  Field_definition(
      Field_type &field, Field_encode_predicate encode_predicate,
      Unknown_field_policy unknown_field_policy = Unknown_field_policy::ignore)
      : Field_definition(field, unknown_field_policy) {
    m_encode_predicate = encode_predicate;
  }

  /// @brief Constructs field definition object
  /// @param[in] field Reference to described field object
  /// @param[in] field_missing_functor Function that is called for this
  /// field in case it is not available in encoded data
  Field_definition(Field_type &field,
                   Field_missing_functor field_missing_functor)
      : Field_definition(field) {
    m_field_missing_functor = field_missing_functor;
  }

  Field_type &get_ref() { return m_ref.get(); }
  const Field_type &get_ref() const { return m_ref.get(); }

  /// @brief Runs "field missing functor" in case field is not available in the
  /// provided encoded data
  void run_field_missing() const { m_field_missing_functor(); }
  /// @brief Runs encode predicate functor
  /// @retval true Encode field
  /// @retval false Do not encode field
  bool run_encode_predicate() const { return m_encode_predicate(); }

  /// @brief Indicates whether this field can be ignored by decoders that
  /// do not know this field
  /// @retval true Decoders that do not recognize this field may ignore it
  /// @retval false Decoders that do not recognize this field should generate
  /// an error if they find this field in the packet
  bool is_field_ignorable() const {
    return m_unknown_field_policy == Unknown_field_policy::ignore;
  }

 protected:
  Field_type_ref m_ref;  ///< Field object reference

  /// Defines what a decoder should do if the field is present but the decoder
  /// is a version that does not know how to interpret the field.
  /// Typically the encoder writes this information to the packet as a
  /// directive that instructs old decoders or third-party decoders how to
  /// handle the field, if the message definition that they are aware of
  /// does not include the field.
  Unknown_field_policy m_unknown_field_policy = Unknown_field_policy::ignore;

  /// Defines whether field will be encoded
  Field_encode_predicate m_encode_predicate;

  /// Function that is called for this field during decoding in case
  /// field is missing in the encoded message
  Field_missing_functor m_field_missing_functor;
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_FIELD_DEFINITION_H
