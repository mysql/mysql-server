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

#ifndef MYSQL_SERIALIZATION_ARCHIVE_TEXT_H
#define MYSQL_SERIALIZATION_ARCHIVE_TEXT_H

#include <sstream>

#include "mysql/serialization/archive.h"
#include "mysql/serialization/serializer.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Archive implementation based on stringstream
/// @note Does not provide backward or forward compatibility, used only
/// for printing text, debug internally
class Archive_text : public Archive<Archive_text> {
 public:
  /// @copydoc Archive::operator<<
  template <typename Field_type>
  Archive_text &operator<<(Field_type &&arg);

  /// @copydoc Archive::operator>>
  template <typename Field_type>
  Archive_text &operator>>(Field_type &&arg);

  /// @copydoc Archive::get_raw_data
  std::string get_raw_data();

  /// @copydoc Archive::get_size_written
  inline std::size_t get_size_written() const {
    return m_stream.str().length();
  }

  // available for serializer concrete types

  /// @copydoc Archive::put_field_separator
  void put_field_separator() override;

  /// @copydoc Archive::put_entry_separator
  void put_entry_separator() override;

  /// @copydoc Archive::put_level_separator
  void put_level_separator() override;

  /// @copydoc Archive::peek
  template <class Field_type>
  void peek(Field_type &&field);

  /// @copydoc Archive::seek_to
  void seek_to(std::size_t num_pos) {
    std::size_t pos = m_stream.tellg();
    m_stream.seekg(pos + num_pos, std::ios_base::beg);
  }

  /// @copydoc Archive::get_size
  template <typename T, Field_size S>
  static std::size_t get_size([[maybe_unused]] Field_wrapper<T, S> &&arg) {
    return 0;  // size info unused
  }

  /// @copydoc Archive::get_size
  template <typename T, Field_size S>
  static constexpr std::size_t get_max_size() {
    return 0;  // size info unused
  }

  /// @copydoc Archive::get_read_pos
  inline std::size_t get_read_pos() const { return 0; }

 private:
  std::stringstream m_stream;  ///< Internal data stream
};

}  // namespace mysql::serialization

/// @}

#include "mysql/serialization/archive_text_impl.hpp"

#endif  // MYSQL_SERIALIZATION_ARCHIVE_TEXT_H
