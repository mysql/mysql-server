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

/// @file concat.h
///
/// Convenience function that concatenates arbitrary arguments, by
/// feeding them to an ostringstream.

#ifndef MYSQL_STRING_CONCAT_H_
#define MYSQL_STRING_CONCAT_H_

#include <sstream>  // std::ostringstream
#include <string>   // std::string

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event::string {

namespace internal {

// Helper for 'concat' (base case)
inline void concat_to_stringstream([[maybe_unused]] std::ostringstream &out) {}

// Helper for 'concat' (recursive)
template <class T, class... Args>
void concat_to_stringstream(std::ostringstream &out, T first,
                            Args... remainder) {
  out << first;
  concat_to_stringstream(out, remainder...);
}

}  // namespace internal

/// Convert all the arguments to strings and concatenate the strings.
///
/// This feeds all arguments to a `std::ostringstream`, so supports all
/// types for which `operator<<(std::ostringstream&, ...)` is defined.
///
/// @param args Arguments to concatenate.
///
/// @return The resulting std::string.
template <class... Args>
std::string concat(Args... args) {
  std::ostringstream out;
  internal::concat_to_stringstream(out, args...);
  return out.str();
}

}  // namespace mysql::binlog::event::string

/// @}

#endif  // MYSQL_STRING_CONCAT_H_
