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
Write_archive_binary &Write_archive_binary::operator<<(Field_type &&arg) {
  static constexpr auto value_size = Field_type::value_size;
  using value_type = typename std::decay_t<Field_type>::value_type;
  if (is_good() == true) {
    bool is_error = true;
    auto bytes_needed =
        Byte_count_helper<value_size>::count_write_bytes(arg.get());
    if (can_write(bytes_needed)) {
      auto bytes_written =
          Primitive_type_codec<value_type>::template write_bytes<value_size>(
              m_stream + m_write_pos, arg.get());
      is_error = bytes_written == 0;
      m_write_pos += bytes_written;
    }
    if (is_error) {
      m_error = Serialization_error(
          __FILE__, __LINE__, "Unable to write data to the stream",
          Serialization_error_type::archive_write_error);
      return *this;
    }
  }
  return *this;
}

}  // namespace mysql::serialization
