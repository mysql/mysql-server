/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mysql/binlog/event/compression/buffer/grow_status.h"

#include <cassert>
#include <string>

namespace mysql::binlog::event::compression::buffer {

const std::string invalid_grow_status_string = "invalid";

std::string debug_string(Grow_status status) {
  switch (status) {
    case Grow_status::success:
      return "success";
    case Grow_status::out_of_memory:
      return "out_of_memory";
    case Grow_status::exceeds_max_size:
      return "exceeds_max_size";
    default:
      break;
  }
  assert(0);
  return invalid_grow_status_string;
}

std::ostream &operator<<(std::ostream &stream, Grow_status status) {
  stream << debug_string(status);
  return stream;
}

}  // namespace mysql::binlog::event::compression::buffer
