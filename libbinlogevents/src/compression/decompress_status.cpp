/* Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <compression/decompressor.h>

namespace binary_log::transaction::compression {

std::string_view debug_string(Decompress_status status) {
  switch (status) {
    case Decompress_status::success:
      return "success";
    case Decompress_status::out_of_memory:
      return "out_of_memory";
    case Decompress_status::exceeds_max_size:
      return "exceeds_max_size";
    case Decompress_status::truncated:
      return "truncated";
    case Decompress_status::corrupted:
      return "corrupted";
    case Decompress_status::end:
      return "end";
    default:
      break;
  }
  assert(0);
  return "[invalid]";
}

std::ostream &operator<<(std::ostream &stream, const Decompress_status status) {
  stream << debug_string(status);
  return stream;
}

}  // namespace binary_log::transaction::compression
