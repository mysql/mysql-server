/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mrs_component_log.h"

namespace mrs_component {

/**
  Emits information entry into the system log
  @param log_type Log type - error or diagnostic log
  @param level    Severity level: info, warning, error
  @param code     Client to server error code (see mysqld_error.h)
  @param msg      Formatted string message
*/
void Log::log_message(enum_log_type log_type, long long level, long long code,
                      const char *msg, ...) const {
  va_list args;
  va_start(args, msg);
  log_message_va(log_type, level, code, msg, args);
  va_end(args);
}

}  // namespace mrs_component
