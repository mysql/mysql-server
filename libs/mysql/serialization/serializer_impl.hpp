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

#include <iostream>

namespace mysql::serialization {

template <class Serializer_derived_type, class Archive_type>
template <class Field_type, Field_size field_size_defined>
void Serializer<Serializer_derived_type, Archive_type>::encode_field(
    Level_type level, Field_id_type field_id,
    const Field_definition<Field_type, field_size_defined> &field_definition) {
  get_derived()->encode(level, field_id, field_definition);
}

template <class Serializer_derived_type, class Archive_type>
template <class Field_type, Field_size field_size_defined>
void Serializer<Serializer_derived_type, Archive_type>::decode_field(
    Level_type level, Field_id_type field_id, std::size_t serializable_end_pos,
    Field_definition<Field_type, field_size_defined> &field_definition) {
  get_derived()->decode(level, field_id, serializable_end_pos,
                        field_definition);
}

template <class Serializer_derived_type, class Archive_type>
template <class Field_type, Field_size field_size_defined>
std::size_t
Serializer<Serializer_derived_type, Archive_type>::get_size_field_def(
    Field_id_type field_id,
    const Field_definition<Field_type, field_size_defined> &field_definition) {
  return Serializer_derived_type::template get_size_field_def(field_id,
                                                              field_definition);
}

template <class Serializer_derived_type, class Archive_type>
template <class Serializable_concrete_type>
std::size_t
Serializer<Serializer_derived_type, Archive_type>::get_size_serializable(
    Field_id_type field_id, const Serializable_concrete_type &serializable,
    bool skip_id) {
  return Serializer_derived_type::template get_size_serializable(
      field_id, serializable, skip_id);
}

template <class Serializer_derived_type, class Archive_type>
template <typename T>
std::size_t Serializer<Serializer_derived_type, Archive_type>::get_size(
    const T &arg) {
  return get_size_serializable(0, arg, false);
}

template <class Serializer_derived_type, class Archive_type>
template <typename T>
Serializer<Serializer_derived_type, Archive_type> &
Serializer<Serializer_derived_type, Archive_type>::operator<<(const T &arg) {
  Field_id_type field_id = serialization_format_version;
  encode_serializable(m_level, field_id, arg, false);
  return *this;
}

template <class Serializer_derived_type, class Archive_type>
template <typename T>
Serializer<Serializer_derived_type, Archive_type>
    &Serializer<Serializer_derived_type, Archive_type>::operator>>(T &arg) {
  Field_id_type field_id = serialization_format_version;
  // passing 0 as serializable_end_pos
  decode_serializable(m_level, field_id, 0, arg, false);
  return *this;
}

template <class Serializer_derived_type, class Archive_type>
Archive_type &Serializer<Serializer_derived_type, Archive_type>::get_archive() {
  return m_archive;
}

template <class Serializer_derived_type, class Archive_type>
template <class Serializable_type>
void Serializer<Serializer_derived_type, Archive_type>::
    encode_serializable_fields(const Serializable_type &serializable,
                               Level_type level) {
  auto process_serializable =
      [ this, level ](const auto &field, auto field_id) -> auto{
    this->encode_serializable(level, field_id, field, false);
  };
  auto process_field =
      [ this, level ](const auto &field, auto field_id) -> auto{
    this->encode_field(level, field_id, field);
    ++field_id;
  };
  serializable.do_for_each_field(process_serializable, process_field);
}

template <class Serializer_derived_type, class Archive_type>
template <class Serializable_type>
void Serializer<Serializer_derived_type, Archive_type>::encode_serializable(
    Level_type level, Field_id_type field_id,
    const Serializable_type &serializable, bool skip_id) {
  if (is_good()) {
    get_derived()->encode_serializable_metadata(level, field_id, serializable,
                                                skip_id);
    if (is_error()) {
      return;
    }
    // restart numbering below for nested type
    // separate enumeration is needed to keep forward compatibility of
    // serialized types, this way if a field is added inside a nested type,
    // it does not affect rest of the class fields ids (they keep the same
    // numbering). Before restarting, serializable must save it's own id
    encode_serializable_fields(serializable, level + 1);
  }
}

template <class Serializer_derived_type, class Archive_type>
template <class Serializable_type>
void Serializer<Serializer_derived_type, Archive_type>::
    decode_serializable_fields(Serializable_type &serializable,
                               Level_type level,
                               std::size_t serializable_end_pos) {
  auto process_serializable =
      [ this, level, serializable_end_pos ](auto &field, auto field_id) -> auto{
    this->decode_serializable(level, field_id, serializable_end_pos, field,
                              false);
  };
  auto process_field =
      [ this, level, serializable_end_pos ](auto &field, auto field_id) -> auto{
    this->decode_field(level, field_id, serializable_end_pos, field);

    ++field_id;
  };
  serializable.do_for_each_field(process_serializable, process_field);
}

template <class Serializer_derived_type, class Archive_type>
template <class Serializable_type>
void Serializer<Serializer_derived_type, Archive_type>::decode_serializable(
    Level_type level, Field_id_type field_id, std::size_t serializable_end_pos,
    Serializable_type &serializable, bool skip_id) {
  // serializable_end_pos = 0 is a marker for serializers that don't check end
  // position and for the top-level serializable field (no parent serializable
  // with end position defined)
  if (skip_id == false && serializable_end_pos != 0 &&
      m_archive.get_read_pos() == serializable_end_pos) {  // reached the end
    // missing field handling here
    // must run field missing functors for all serializable fields recursively
    auto run_field_missing_functor = [](auto &field, auto) {
      field.run_field_missing();
    };
    auto serializable_field_missing_functor = [](auto &, auto) {};
    serializable.do_for_each_field(serializable_field_missing_functor,
                                   run_field_missing_functor);
    return;
  }

  if (is_good()) {
    auto curr_pos = m_archive.get_read_pos();
    auto encoded_size_read = get_derived()->decode_serializable_metadata(
        level, field_id, serializable, skip_id);
    if (is_error()) {
      return;
    }
    // restart numbering below for nested type
    // separate enumeration is needed to keep forward compatibility of
    // serialized types, this way if a field is added inside a nested type,
    // it does not affect rest of the class fields ids (they keep the same
    // numbering). Before restarting, serializable must save it's own id
    decode_serializable_fields(serializable, level + 1,
                               curr_pos + encoded_size_read);

    auto next_pos = m_archive.get_read_pos();
    auto expected_pos = curr_pos + encoded_size_read;
    if (expected_pos > next_pos && is_good()) {
      m_archive.seek_to(expected_pos - next_pos);
    }
  }
}

}  // namespace mysql::serialization
