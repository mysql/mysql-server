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

/// @defgroup GroupLibsMysqlSerialization MySQL Libraries : Serialization
/// @ingroup GroupLibsMysql

#ifndef MYSQL_SERIALIZATION_SERIALIZATION_ERROR_H
#define MYSQL_SERIALIZATION_SERIALIZATION_ERROR_H

/// @file
/// Experimental API header

#include <exception>
#include <sstream>
#include <string>

#include "mysql/serialization/serialization_error_type.h"
#include "mysql/utils/error.h"

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Error used internally in serialization framework
class Serialization_error : public utils::Error {
 public:
  /// @brief Constructor
  /// @param[in] file File name in which exception occurred
  /// @param[in] line Line number in which exception occurred
  /// @param[in] message Additional information
  /// @param[in] error_type Type of error
  Serialization_error(const char *file, std::size_t line, const char *message,
                      const Serialization_error_type &error_type);

  Serialization_error() = default;

  /// @brief Error type accessor
  /// @return Error type
  const Serialization_error_type &get_type() const;

 private:
  Serialization_error_type m_type;  ///< Error type
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_SERIALIZATION_ERROR_H
