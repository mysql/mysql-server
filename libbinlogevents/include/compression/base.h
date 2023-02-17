/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef LIBBINLOGEVENTS_COMPRESSION_BASE_H_
#define LIBBINLOGEVENTS_COMPRESSION_BASE_H_

#include <string>

namespace binary_log::transaction::compression {

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

}  // namespace binary_log::transaction::compression

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_BASE_H_
