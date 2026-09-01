/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_log.h"

SERVICE_TYPE(log_builtins) * log_bi{nullptr};
SERVICE_TYPE(log_builtins_string) * log_bs{nullptr};

namespace hm {

/**
  Initialize the globals for the MySQL logging component.
*/
Log::Log() {
  log_bi = log_srv;
  log_bs = log_string_srv;
}

/**
  Emits information entry into the system log
  @param log_type Log type - error or diagnostic log
  @param level    Severity level: info, warning, error
  @param code     Client to server error code (see mysqld_error.h)
  @param msg      Formatted string message
*/
void Log::log_message(enum_log_type log_type, long long level, long long code,
                      const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  log_message_va(log_type, level, code, msg, args);
  va_end(args);
}

/**
 Boolean to string helper functions
 @param value    boolean
 @param if_true  String if 'value' is true
 @param if_false String if 'value' is false
 @return string boolean
*/
std::string bool_to_string(bool value, const std::string &if_true,
                           const std::string &if_false) {
  return (value ? if_true : if_false);
}
std::string on_off(bool value) { return bool_to_string(value, "ON", "OFF"); }
std::string true_false(bool value) {
  return bool_to_string(value, "TRUE", "FALSE");
}

}  // namespace hm