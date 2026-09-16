/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#pragma once

#include <cerrno>
#include <cstdint>
#include <limits>
#include <system_error>

namespace mysql::system_info::internal {

/** Return the current errno value in the generic error category. */
[[nodiscard]] inline std::error_code errno_code() {
  return {errno, std::generic_category()};
}

/** Return true if multiplying two unsigned 64-bit values would overflow. */
[[nodiscard]] inline bool multiply_overflows(uint64_t lhs, uint64_t rhs) {
  return rhs != 0 && lhs > std::numeric_limits<uint64_t>::max() / rhs;
}

/** Return true if adding two unsigned 64-bit values would overflow. */
[[nodiscard]] inline bool add_overflows(uint64_t lhs, uint64_t rhs) {
  return rhs > std::numeric_limits<uint64_t>::max() - lhs;
}

}  // namespace mysql::system_info::internal
