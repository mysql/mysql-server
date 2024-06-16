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

/// @file
/// Experimental API header

#include "mysql/utils/return_status.h"

namespace mysql::serialization {

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field) {
  m_archive << Field_wrapper<const Field_type, field_size_defined>(field);
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field) {
  m_archive >> Field_wrapper<Field_type, field_size_defined>(field);
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_enum_tag) {
  auto enumeration_value_encoded = utils::to_underlying(field);
  using Enumeration_type = decltype(enumeration_value_encoded);
  m_archive << Field_wrapper<Enumeration_type, field_size_defined>(
      enumeration_value_encoded);
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_enum_tag) {
  auto enumeration_value_encoded = utils::to_underlying(field);
  using Enumeration_type = decltype(enumeration_value_encoded);
  m_archive >> Field_wrapper<Enumeration_type, field_size_defined>(
                   enumeration_value_encoded);
  auto [enumeration_value_decoded, conversion_code] =
      mysql::utils::to_enumeration<Field_type, Enumeration_type>(
          enumeration_value_encoded);
  field = enumeration_value_decoded;
  if (conversion_code == utils::Return_status::error) {
    m_error =
        Serialization_error(__FILE__, __LINE__, "Data integrity error",
                            Serialization_error_type::data_integrity_error);
  }
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_vector_list_tag) {
  uint64_t encoded_size = field.size();
  m_archive << create_varlen_field_wrapper(encoded_size);
  using value_type = typename Field_type::value_type;
  for (const auto &internal_field : field) {
    m_archive.put_entry_separator();
    // we use default size for internal fields (0)
    encode_field<value_type, 0>(internal_field);
  }
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_vector_list_tag) {
  uint64_t encoded_size;
  m_archive >> create_varlen_field_wrapper(encoded_size);
  field.clear();
  using value_type = typename Field_type::value_type;
  for (uint64_t internal_id = 0; internal_id < encoded_size; ++internal_id) {
    value_type internal_field;
    // we use default size for internal fields (0)
    decode_field<value_type, 0>(internal_field);
    field.push_back(internal_field);
    m_archive.process_entry_separator();
  }
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_serializable_tag) {
  Serializer_default<Archive_concrete_type>::encode_serializable(0, 0, field,
                                                                 true);
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_serializable_tag) {
  // For serializable inside container, we don't check serializable id,
  // since it must be there. Passing 0 as serializable_end_pos.
  Serializer_default<Archive_concrete_type>::decode_serializable(0, 0, 0, field,
                                                                 true);
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_map_tag) {
  uint64_t encoded_size = field.size();
  m_archive << create_varlen_field_wrapper(encoded_size);
  for (auto &pair : field) {
    using key_type = typename Field_type::key_type;
    using mapped_type = typename Field_type::mapped_type;
    // cannot choose fixed number of bytes for map key type and mapped type,
    // we use default size for internal fields (0)
    encode_field<key_type, 0>(pair.first);
    m_archive.put_entry_separator();
    encode_field<mapped_type, 0>(pair.second);
    m_archive.put_entry_separator();
  }
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_map_tag) {
  uint64_t encoded_size;
  m_archive >> create_varlen_field_wrapper(encoded_size);
  field.clear();
  for (uint64_t entry_id = 0; entry_id < encoded_size; ++entry_id) {
    using key_type = typename Field_type::key_type;
    using mapped_type = typename Field_type::mapped_type;
    key_type key;
    mapped_type value;
    // cannot choose fixed number of bytes for map key type and mapped type,
    // we use default size for internal fields (0)
    decode_field<key_type, 0>(key);
    m_archive.process_entry_separator();
    decode_field<mapped_type, 0>(value);
    m_archive.process_entry_separator();
    field.insert(std::make_pair(key, value));
  }
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_set_tag) {
  uint64_t encoded_size = field.size();
  m_archive << create_varlen_field_wrapper(encoded_size);
  using value_type = typename Field_type::value_type;
  for (const auto &internal_field : field) {
    // we use default size for internal fields (0)
    encode_field<value_type, 0>(internal_field);
    m_archive.put_entry_separator();
  }
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_set_tag) {
  uint64_t encoded_size;
  m_archive >> create_varlen_field_wrapper(encoded_size);
  using value_type = typename Field_type::value_type;
  field.clear();
  for (uint64_t internal_id = 0; internal_id < encoded_size; ++internal_id) {
    value_type internal_field;
    // we use default size for internal fields (0)
    decode_field<value_type, 0>(internal_field);
    field.emplace(internal_field);
    m_archive.process_entry_separator();
  }
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::encode_field(
    const Field_type &field, Serializer_array_tag) {
  using value_type = std::remove_reference_t<decltype(*std::begin(
      std::declval<Field_type &>()))>;
  for (const auto &internal_field : field) {
    // we use default size for internal fields (0)
    encode_field<value_type, 0>(internal_field);
    m_archive.put_entry_separator();
  }
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
void Serializer_default<Archive_concrete_type>::decode_field(
    Field_type &field, Serializer_array_tag) {
  using value_type = std::remove_reference_t<decltype(*std::begin(
      std::declval<Field_type &>()))>;
  for (auto &internal_field : field) {
    // we use default size for internal fields (0)
    decode_field<value_type, 0>(internal_field);
    m_archive.process_entry_separator();
  }
  m_archive.process_field_separator();
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field) {
  return Archive_concrete_type::template get_size(
      Field_wrapper<const Field_type, field_size_defined>(field));
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_enum_tag) {
  auto enumeration_value_encoded = utils::to_underlying(field);
  using Enumeration_type = decltype(enumeration_value_encoded);
  return get_field_size<Enumeration_type, field_size_defined>(
      enumeration_value_encoded);
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_vector_list_tag) {
  std::size_t field_size = 0;
  using value_type = typename Field_type::value_type;
  uint64_t encoded_size = field.size();
  field_size += get_field_size<uint64_t, 0>(encoded_size);
  for (const auto &internal_field : field) {
    field_size += get_field_size<value_type, 0>(internal_field);
  }
  return field_size;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_serializable_tag) {
  return Serializer_default<Archive_concrete_type>::get_size_serializable(
      0, field, true);
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_map_tag) {
  using key_type = typename Field_type::key_type;
  using mapped_type = typename Field_type::mapped_type;
  std::size_t field_size = 0;
  uint64_t encoded_size = field.size();
  field_size += get_field_size<uint64_t, 0>(encoded_size);
  for (auto &pair : field) {
    field_size += get_field_size<key_type, 0>(pair.first);
    field_size += get_field_size<mapped_type, 0>(pair.second);
  }
  return field_size;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_set_tag) {
  std::size_t field_size = 0;
  uint64_t encoded_size = field.size();
  field_size += get_field_size<uint64_t, 0>(encoded_size);
  using value_type = typename Field_type::value_type;
  for (const auto &internal_field : field) {
    field_size += get_field_size<value_type, 0>(internal_field);
  }
  return field_size;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined, typename Enabler>
std::size_t Serializer_default<Archive_concrete_type>::get_field_size(
    const Field_type &field, Serializer_array_tag) {
  std::size_t field_size = 0;
  using value_type = std::remove_reference_t<decltype(*std::begin(
      std::declval<Field_type &>()))>;
  for (const auto &internal_field : field) {
    field_size += get_field_size<value_type, 0>(internal_field);
  }
  return field_size;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined>
bool Serializer_default<Archive_concrete_type>::encode_field_id(
    Level_type level, Field_id_type field_id,
    const Field_definition<Field_type, field_size_defined> &field_definition) {
  bool is_provided = false;
  if (this->is_good() == true) {
    is_provided = field_definition.run_encode_predicate();
    if (is_provided) {
      for (size_t level_id = 0; level_id < level; ++level_id) {
        m_archive.put_level_separator();
      }
      m_archive << create_varlen_field_wrapper(field_id);
      m_archive.put_entry_separator();
    }
  }
  return is_provided;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined>
bool Serializer_default<Archive_concrete_type>::decode_field_id(
    Level_type level, Field_id_type field_id,
    Field_definition<Field_type, field_size_defined> &field_definition) {
  bool is_provided = false;
  if (this->is_good() == true) {
    is_provided = true;
    for (size_t level_id = 0; level_id < level; ++level_id) {
      m_archive.process_level_separator();
    }
    Field_id_type field_type_read;
    m_archive.peek(create_varlen_field_wrapper(field_type_read));
    if (field_id != field_type_read || this->is_good() == false) {
      field_definition.run_field_missing();
      is_provided = false;
    }
    if (is_provided) {
      m_archive >> create_varlen_field_wrapper(field_type_read);
      m_archive.process_entry_separator();
    }
  }
  return is_provided;
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined>
void Serializer_default<Archive_concrete_type>::encode(
    Level_type level, Field_id_type field_id,
    const Field_definition<Field_type, field_size_defined> &field_definition) {
  bool is_provided = encode_field_id(level, field_id, field_definition);
  if (is_provided) {
    encode_field<Field_type, field_size_defined>(field_definition.get_ref());
  }
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined>
void Serializer_default<Archive_concrete_type>::decode(
    Level_type level, Field_id_type field_id, std::size_t serializable_end_pos,
    Field_definition<Field_type, field_size_defined> &field_definition) {
  // serializable_end_pos = 0 is a marker for serializers that don't check end
  // pos
  if (m_archive.get_read_pos() == serializable_end_pos &&
      serializable_end_pos != 0) {  // reached the end
    // missing field handling here
    field_definition.run_field_missing();
    return;
  }
  bool is_provided = decode_field_id(level, field_id, field_definition);
  if (is_provided) {
    decode_field<Field_type, field_size_defined>(field_definition.get_ref());
  }
}

template <class Serializable_type>
Field_id_type find_last_non_ignorable_field_id(
    const Serializable_type &serializable) {
  Field_id_type last_non_ignorable_field_id = 0;
  auto func_s = [&last_non_ignorable_field_id](const auto &nested,
                                               auto nested_id) {
    if (nested.is_any_field_provided() && nested.is_ignorable() == false) {
      last_non_ignorable_field_id = nested_id + 1;
    }
  };
  auto func_f = [&last_non_ignorable_field_id](const auto &field,
                                               auto processed_field_id) -> auto{
    if (field.run_encode_predicate() && field.is_field_ignorable() == false) {
      last_non_ignorable_field_id = processed_field_id + 1;
    }
  };
  serializable.do_for_each_field(func_s, func_f);
  return last_non_ignorable_field_id;
}

template <class Archive_concrete_type>
template <class Serializable_type>
void Serializer_default<Archive_concrete_type>::encode_serializable_metadata(
    Level_type, Field_id_type field_id, const Serializable_type &serializable,
    bool skip_id) {
  using Serializer_type = Serializer_default<Archive_concrete_type>;

  uint64_t encoded_size =
      Serializer_type::get_size_serializable(field_id, serializable, skip_id);

  Field_id_type last_non_ignorable_field_id =
      find_last_non_ignorable_field_id(serializable);

  if (skip_id == false) {
    m_archive << create_varlen_field_wrapper(field_id);
    m_archive.put_field_separator();
  }
  m_archive << create_varlen_field_wrapper(encoded_size);
  m_archive.put_field_separator();
  m_archive << create_varlen_field_wrapper(last_non_ignorable_field_id);
  m_archive.put_field_separator();
}

template <class Archive_concrete_type>
template <class Serializable_type>
std::size_t
Serializer_default<Archive_concrete_type>::decode_serializable_metadata(
    Level_type, Field_id_type field_id, Serializable_type &serializable,
    bool skip_id) {
  uint64_t encoded_size = 0;
  Field_id_type last_non_ignorable_field_id = 0;
  if (skip_id == false) {
    Field_id_type field_type_read;
    m_archive >> create_varlen_field_wrapper(field_type_read);
    m_archive.process_field_separator();
    if (field_type_read != field_id || this->is_good() == false) {
      m_error =
          Serialization_error(__FILE__, __LINE__, "Missing field id",
                              Serialization_error_type::field_id_mismatch);
    }
  }

  if (this->is_good()) {
    auto last_known_field_id = serializable.get_last_field_id();
    m_archive >> create_varlen_field_wrapper(encoded_size);
    m_archive.process_field_separator();
    m_archive >> create_varlen_field_wrapper(last_non_ignorable_field_id);
    m_archive.process_field_separator();

    if (last_known_field_id + 1 < last_non_ignorable_field_id) {
      m_error = Serialization_error(
          __FILE__, __LINE__,
          "Unknown, non ignorable fields found in the data stream.",
          Serialization_error_type::unknown_field);
    }
  }
  return static_cast<std::size_t>(encoded_size);
}

template <class Archive_concrete_type>
template <class Field_type, Field_size field_size_defined>
std::size_t Serializer_default<Archive_concrete_type>::get_size_field_def(
    Field_id_type field_id,
    const Field_definition<Field_type, field_size_defined> &field_definition) {
  std::size_t calculated_size = 0;
  bool is_provided = field_definition.run_encode_predicate();
  if (is_provided) {
    auto size_id_type = Archive_concrete_type::template get_size(
        create_varlen_field_wrapper(field_id));
    calculated_size = get_field_size<Field_type, field_size_defined>(
                          field_definition.get_ref()) +
                      size_id_type;
  }
  return calculated_size;
}

template <class Archive_concrete_type>
template <class Serializable_concrete_type>
std::size_t Serializer_default<Archive_concrete_type>::get_size_serializable(
    Field_id_type field_id, const Serializable_concrete_type &serializable,
    bool skip_id) {
  std::size_t serializable_overhead_type = 0;
  if (skip_id == false) {
    serializable_overhead_type = Archive_concrete_type::template get_size(
        create_varlen_field_wrapper(field_id));
  }
  auto serializable_size = serializable.template get_size_internal<Base_type>();
  auto serializable_overhead_size = Archive_concrete_type::template get_size(
      create_varlen_field_wrapper(serializable_size));

  Field_id_type last_non_ignorable_field_id =
      find_last_non_ignorable_field_id(serializable);

  auto serializable_overhead_last_non_ignorable_field_id =
      Archive_concrete_type::template get_size(
          create_varlen_field_wrapper(last_non_ignorable_field_id));
  return serializable_overhead_type + serializable_overhead_size +
         serializable_overhead_last_non_ignorable_field_id + serializable_size;
}

}  // namespace mysql::serialization
