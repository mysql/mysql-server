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

#include "mysql/serialization/write_archive_binary.h"

/// @file

namespace mysql::serialization {

Write_archive_binary::Internal_type &Write_archive_binary::get_raw_data() {
  return m_stream;
}

bool Write_archive_binary::can_write(size_t bytes) const {
  if (is_good() == true && (m_write_pos + bytes <= m_stream_size)) return true;
  return false;
}

std::size_t Write_archive_binary::get_size_written() const {
  return m_write_pos;
}

}  // namespace mysql::serialization
