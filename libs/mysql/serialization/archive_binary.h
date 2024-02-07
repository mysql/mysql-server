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

#ifndef MYSQL_SERIALIZATION_ARCHIVE_BINARY_H
#define MYSQL_SERIALIZATION_ARCHIVE_BINARY_H

#include <sstream>

#include "mysql/serialization/archive.h"
#include "mysql/serialization/archive_binary_field_max_size_calculator.h"
#include "mysql/serialization/primitive_type_codec.h"
#include "mysql/serialization/serializable_type_traits.h"
#include "mysql/serialization/serialization_types.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Binary archive implementation based on vector of bytes
class Archive_binary : public Archive<Archive_binary> {
 public:
  /// @copydoc Archive::operator<<
  template <typename Field_type>
  Archive_binary &operator<<(Field_type &&arg);

  /// @copydoc Archive::operator>>
  template <typename Field_type>
  Archive_binary &operator>>(Field_type &&arg);

  /// @copydoc Archive::get_max_size
  template <typename Field_type, Field_size defined_field_size>
  static constexpr std::size_t get_max_size() {
    return Archive_binary_field_max_size_calculator<
        Field_type, defined_field_size>::get_max_size();
  }

  /// @copydoc Archive::get_size
  template <typename Field_type>
  static std::size_t get_size(Field_type &&arg) {
    static constexpr auto value_size = Field_type::value_size;
    using value_type = typename std::decay_t<Field_type>::value_type;
    return Primitive_type_codec<value_type>::template count_write_bytes<
        value_size>(arg.get());
  }

  /// @copydoc Archive::get_raw_data
  std::vector<unsigned char> &get_raw_data();

  /// @copydoc Archive::peek
  template <class Field_type>
  void peek(Field_type &&field);

  /// @copydoc Archive::seek_to
  void seek_to(std::size_t num_pos) { read_pos += num_pos; }

  /// @copydoc Archive::get_read_pos
  inline std::size_t get_read_pos() const { return read_pos; }

 private:
  std::vector<unsigned char> m_stream;  ///< Internal data stream
  std::size_t read_pos{0};              ///< Read position
};

}  // namespace mysql::serialization

/// @}

#include "mysql/serialization/archive_binary_impl.hpp"

#endif  // MYSQL_SERIALIZATION_ARCHIVE_BINARY_H
