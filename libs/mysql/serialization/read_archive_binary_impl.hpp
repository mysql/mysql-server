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

namespace mysql::serialization {

template <typename Field_type>
Read_archive_binary &Read_archive_binary::operator>>(Field_type &&arg) {
  static constexpr auto value_size = Field_type::value_size;
  using value_type = typename std::decay_t<Field_type>::value_type;
  if (is_good() == true) {
    auto bytes_read =
        Primitive_type_codec<value_type>::template read_bytes<value_size>(
            m_stream + read_pos, m_stream_size - read_pos, arg.get());
    if (bytes_read == 0) {
      m_error = Serialization_error(
          __FILE__, __LINE__, "Unable to read data from the stream",
          Serialization_error_type::archive_read_error);
      return *this;
    }
    read_pos += bytes_read;
  }
  return *this;
}

template <class Field_type>
void Read_archive_binary::peek(Field_type &&field) {
  auto read_pos_saved = read_pos;
  this->operator>>(std::forward<Field_type>(field));
  read_pos = read_pos_saved;
}

}  // namespace mysql::serialization
