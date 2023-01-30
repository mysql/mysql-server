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

/// @addtogroup Replication
/// @{
///  @file grow_status.h

#ifndef MYSQL_BUFFER_GROW_STATUS_H_
#define MYSQL_BUFFER_GROW_STATUS_H_

#include <ostream>
#include <string>

namespace mysqlns::buffer {

/// Error statuses for classes that use Grow_calculator.
enum class Grow_status {
  /// A grow operation succeeded. The data structure is now of the new
  /// size.
  success,
  /// A grow operation could not be performed because there is a
  /// configured maximum size. The data structure is unchanged.
  exceeds_max_size,
  /// A grow operation failed because memory allocation failed. The
  /// data structure is unchanged.
  out_of_memory
};

/// Return value from debug_string(Grow_status) when the parameter is
/// not a valid value.
extern const std::string invalid_grow_status_string;

/// Return a string that describes each enumeration value.
std::string debug_string(Grow_status status);

/// Write a string that describes the enumeration value to the stream.
std::ostream &operator<<(std::ostream &stream, Grow_status status);

}  // namespace mysqlns::buffer

/// @} (end of group Replication)

#endif /* MYSQL_BUFFER_GROW_STATUS_H_ */
