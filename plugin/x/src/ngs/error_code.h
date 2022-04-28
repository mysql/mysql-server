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

#ifndef PLUGIN_X_SRC_NGS_ERROR_CODE_H_
#define PLUGIN_X_SRC_NGS_ERROR_CODE_H_

#include <stdio.h>
#include <cstdarg>
#include <string>

#include "my_compiler.h"   // NOLINT(build/include_subdir)
#include "my_dbug.h"       // NOLINT(build/include_subdir)
#include "my_sys.h"        // NOLINT(build/include_subdir)
#include "mysqld_error.h"  // NOLINT(build/include_subdir)

namespace ngs {

struct Error_code {
  static const int MAX_MESSAGE_LENGTH = 1024;

  int error;
  std::string message;
  std::string sql_state;
  enum Severity {
    OK = 0,
    ERROR = 1,
    FATAL = 2,
  } severity;

  Error_code() : error(0), severity(OK) {}
  Error_code(int e, const std::string &m, const std::string &state = "HY000",
             Severity sev = ERROR)
      : error(e), message(m), sql_state(state), severity(sev) {
    if (e) {
      DBUG_PRINT("info", ("Error_code: %s", m.c_str()));
    }
  }

  Error_code(int e, const std::string &state, Severity sev, const char *fmt,
             va_list args) MY_ATTRIBUTE((format(printf, 5, 0)));

  Error_code(const Error_code &o) { operator=(o); }

  Error_code &operator=(const Error_code &o) {
    if (this != &o) {
      error = o.error;
      message = o.message;
      sql_state = o.sql_state;
      severity = o.severity;
    }
    return *this;
  }

  operator bool() const { return error != 0; }
};

inline Error_code::Error_code(int e, const std::string &state, Severity sev,
                              const char *fmt, va_list args)
    : error(e), sql_state(state), severity(sev) {
  char buffer[MAX_MESSAGE_LENGTH];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  message = buffer;
  if (e) {
    DBUG_PRINT("info", ("Error_code: %s", message.c_str()));
  }
}

inline Error_code Success(const char *msg, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));
inline Error_code Error(int e, const char *msg, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
inline Error_code Fatal(int e, const char *msg, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));

inline Error_code Success(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Error_code tmp(Error_code(0, "", Error_code::OK, msg, ap));
  va_end(ap);
  return tmp;
}

inline Error_code Success() { return Error_code(); }

inline Error_code SQLError(const int error_code, ...) {
  va_list ap;
  va_start(ap, error_code);
  const auto format = my_get_err_msg(error_code);

  Error_code tmp(error_code, "");

  if (nullptr != format)
    tmp = Error_code(error_code, "HY000", Error_code::ERROR, format, ap);

  va_end(ap);

  return tmp;
}

inline Error_code SQLError_access_denied() {
  return Error_code(ER_ACCESS_DENIED_ERROR, "Invalid user or password");
}

inline Error_code Error(int e, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Error_code tmp(Error_code(e, "HY000", Error_code::ERROR, msg, ap));
  va_end(ap);
  return tmp;
}

inline Error_code Fatal(int e, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Error_code tmp(Error_code(e, "HY000", Error_code::FATAL, msg, ap));
  va_end(ap);
  return tmp;
}

inline Error_code Fatal(const Error_code &err) {
  Error_code error(err);
  error.severity = Error_code::FATAL;
  return error;
}
}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_ERROR_CODE_H_
