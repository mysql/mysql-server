/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XERROR_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XERROR_H_

#include <string>

#include "mysqlxclient/mysqlxclient_error.h"

namespace xcl {

/**
  MySQL error holder.

  The class can holds error codes from:

  * MySQL Server, errors beginning with ER_ prefix
  * client side, errors beginning with CR_ prefix
  * X Protocol, errors beginning with ER_X_ prefix
  * X Client, errors beginning with CR_X_ prefix

  Object created with default constructor sets the error
  code to "0", which means no error.
*/
class XError {
 public:
  XError() : m_error(0), m_is_fatal(false) {}

  explicit XError(const int err, const std::string &message = "",
                  bool is_fatal = false, const std::string &sql_state = "")
      : m_message(message),
        m_error(err),
        m_is_fatal(is_fatal),
        m_sql_state(sql_state) {}

  /** Check if an error occurred */
  operator bool() const { return 0 != m_error; }

  /** Get error code. */
  int error() const { return m_error; }

  /** Get error description. */
  const char *what() const { return m_message.c_str(); }

  /** Check if error is marked as fatal. */
  bool is_fatal() const { return m_is_fatal; }

  /** Get sql state description. */
  const char *sql_state() const { return m_sql_state.c_str(); }

 private:
  std::string m_message;
  int m_error;
  bool m_is_fatal;
  std::string m_sql_state;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XERROR_H_
