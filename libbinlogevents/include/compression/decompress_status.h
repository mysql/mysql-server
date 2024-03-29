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

#ifndef LIBBINLOGEVENTS_COMPRESSION_DECOMPRESS_STATUS_H_
#define LIBBINLOGEVENTS_COMPRESSION_DECOMPRESS_STATUS_H_

#include <ostream>
#include <string>

namespace binary_log::transaction::compression {

enum class Decompress_status {
  success,           ///< Decompression succeeded.
  out_of_memory,     ///< Memory allocation failed.
  exceeds_max_size,  ///< Requested size exceeds configured maximum.
  truncated,         ///< There were remaining bytes,
                     /// but less than requested.
  corrupted,         ///< Compression library reported an error.
  end                ///< Zero remaining bytes.
};

std::string_view debug_string(Decompress_status status);
std::ostream &operator<<(std::ostream &stream, Decompress_status status);

}  // namespace binary_log::transaction::compression

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_DECOMPRESS_STATUS_H_
