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

#ifndef MYSQL_UTILS_ERROR_H
#define MYSQL_UTILS_ERROR_H

/// @file
/// Experimental API header

#include <memory>
#include <sstream>
#include <string>

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

class Error;

using Error_ptr = std::unique_ptr<mysql::utils::Error>;

/// @brief Error representation used internally in case final error code
/// is unknown and error situation handling needs to be done by the caller
class Error {
 public:
  Error() = default;

  /// @brief Constructor
  /// @param[in] type Information about error type
  /// @param[in] file File name in which error occurred
  /// @param[in] line Line number in which error occurred
  Error(const char *type, const char *file, std::size_t line)
      : Error(type, file, line, "") {}

  /// @brief Constructor
  /// @param[in] type Information about error type
  /// @param[in] file File name in which error occurred
  /// @param[in] line Line number in which error occurred
  /// @param[in] message Additional information
  Error(const char *type, [[maybe_unused]] const char *file,
        [[maybe_unused]] std::size_t line, const char *message) {
#ifndef NDEBUG
    m_stream << type << " error occurred in file: " << file
             << " line: " << line;
    m_stream << "; Message: " << message;
#else
    m_stream << type << ": " << message;
#endif
    m_message = m_stream.str();
    m_user_message = message;
    m_is_error = true;
  }

  /// @brief Function that indicates whether error occurred
  bool is_error() const { return m_is_error; }

  /// @brief Information about error
  /// @return Message const string
  const char *what() const noexcept { return m_message.c_str(); }

  /// @brief Returns only message, no other information
  /// @return Message const string
  const char *get_message() const noexcept { return m_user_message; }

 protected:
  std::stringstream m_stream;  ///< Internal stream to build the message string
  std::string m_message;       ///< Message ready to be displayed
  const char *m_user_message;  ///< Only message
  bool m_is_error = false;     ///< object state, "false" means "no error"
};

}  // namespace mysql::utils

/// @}

#endif  // MYSQL_UTILS_ERROR_H
