#ifndef EXPECTED_ERRORS_INCLUDED
#define EXPECTED_ERRORS_INCLUDED

// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file
///
/// This file declares the Error class.

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "mysql_com.h"  // SQLSTATE_LENGTH

#define MAX_ERROR_COUNT 20

enum error_type { ERR_ERRNO = 1, ERR_SQLSTATE };

/// Class representing an error.
///
/// Contains following information
///   * Error code
///   * Error type
///   * SQLSTATE
///
/// If an error type value is
///   * ERR_ERRNO, then SQLSTATE value is "00000"
///   * ERR_SQLSTATE, then error code value is 0
class Error {
 public:
  Error(std::uint32_t error_code, const char *sqlstate, error_type type) {
    this->m_error_code = error_code;
    this->m_type = type;
    std::strcpy(this->m_sqlstate, sqlstate);
  }

  /// Return a sqlstate for an error.
  ///
  /// @retval SQLSTATE string
  const char *sqlstate() { return m_sqlstate; }

  /// Return an error code
  ///
  /// @retval Error code
  std::uint32_t error_code() { return m_error_code; }

  /// Return an error type
  ///
  /// @retval Error type (ERR_ERRNO or ERR_SQLSTATE)
  error_type type() { return m_type; }

 private:
  char m_sqlstate[SQLSTATE_LENGTH + 1];  // '\0' terminated string
  error_type m_type;
  std::uint32_t m_error_code;
};

/// Class representing a list of error codes passed as argument to
/// mysqltest command <code>--error</code>.
class Expected_errors {
 public:
  typedef std::vector<std::unique_ptr<Error>>::iterator iterator;

  Expected_errors() {}
  ~Expected_errors() {}

  iterator begin() { return m_errors.begin(); }
  iterator end() { return m_errors.end(); }

  /// Return a sqlstate of the first error in the list.
  ///
  /// @retval SQLSTATE string
  const char *sqlstate() { return m_errors[0]->sqlstate(); }

  /// Return an error type of the first error in the list.
  ///
  /// @retval Error type (ERR_ERRNO or ERR_SQLSTATE)
  error_type type() { return m_errors[0]->type(); }

  /// Return length of the list containing errors.
  ///
  /// @retval Length value
  std::size_t count() { return m_errors.size(); }

  /// Return list of error codes
  std::string error_list();

  /// Return an error code of the first error in the list.
  ///
  /// @retval Error code
  unsigned int error_code() { return m_errors[0]->error_code(); }

  /// Return list of error codes in the list
  ///
  /// @retval List of error codes
  std::vector<std::uint32_t> errors();

  /// Add a new error to the existing list of errors.
  ///
  /// @param error_code Error number
  /// @param sqlstate   SQLSTATE string
  /// @param type       Error type
  void add_error(std::uint32_t error_code, const char *sqlstate,
                 error_type type);

  /// Delete all errors from the vector.
  void clear_error_list() { m_errors.clear(); }

 private:
  // List containing expected errors
  std::vector<std::unique_ptr<Error>> m_errors;
};

#endif  // EXPECTED_ERRORS_INCLUDED
