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

#ifndef MYSQL_SERIALIZATION_SERIALIZATION_ERROR_TYPE_H
#define MYSQL_SERIALIZATION_SERIALIZATION_ERROR_TYPE_H

#include <cstdint>
#include <string>

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Possible error codes generated in serialization library
enum class Serialization_error_type : uint64_t {
  archive_read_error,    ///< Cannot read data from the stream
  archive_write_error,   ///< Cannot write data to the stream
  corrupted_field,       ///< cannot read field, corrupted stream
  out_of_memory,         ///< cannot allocate memory
  unknown_field,         ///< unknown field found in the data stream
  field_id_mismatch,     ///< field id read does not match expected field id
  data_integrity_error,  ///< corrupted data, unexpected value found
  last
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_SERIALIZATION_ERROR_TYPE_H
