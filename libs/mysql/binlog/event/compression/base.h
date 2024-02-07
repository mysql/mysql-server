/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_BASE_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_BASE_H

#include <string>

namespace mysql::binlog::event::compression {

// ZSTD version boundary below which instrumented
// ZSTD_create[CD]Stream_advanced functions are allowed.
// When new ZSTD versions are released and they do not
// break prototype for ZSTD_create[CD]Stream_advanced
// increase the number '10505' accordingly
constexpr unsigned int ZSTD_INSTRUMENTED_BELOW_VERSION = 10505;

// Todo: use enum class and a more specific name.
// Use contiguous values.
enum type {
  /* ZSTD compression. */
  ZSTD = 0,

  /* No compression. */
  NONE = 255,
};

template <class T>
bool type_is_valid(T t) {
  switch (t) {
    case ZSTD:
      return true;
    case NONE:
      return true;
    default:
      break;
  }
  return false;
}

std::string type_to_string(type t);

}  // namespace mysql::binlog::event::compression

namespace [[deprecated]] binary_log {
namespace [[deprecated]] transaction {
namespace [[deprecated]] compression {
using namespace mysql::binlog::event::compression;
}  // namespace compression
}  // namespace transaction
}  // namespace binary_log

#endif  // MYSQL_BINLOG_EVENT_COMPRESSION_BASE_H
