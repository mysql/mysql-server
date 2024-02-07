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

#include "sql/server_component/mysql_simple_error_log_imp.h"
#include <stdarg.h>
#include "mysql/components/service.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/mysql_simple_error_log.h"
#include "mysql/my_loglevel.h"

DEFINE_BOOL_METHOD(mysql_simple_error_log_imp::emit,
                   (const char *component, const char *file, unsigned long line,
                    int severity, int error_id, ...)) {
  va_list args;
  loglevel lvl;

  switch (severity) {
    case MYSQL_ERROR_LOG_SEVERITY_SYSTEM:
      lvl = SYSTEM_LEVEL;
      break;
    case MYSQL_ERROR_LOG_SEVERITY_ERROR:
      lvl = ERROR_LEVEL;
      break;
    case MYSQL_ERROR_LOG_SEVERITY_WARNING:
      lvl = WARNING_LEVEL;
      break;
    case MYSQL_ERROR_LOG_SEVERITY_INFORMATION:
      lvl = INFORMATION_LEVEL;
      break;
    default:
      return true;
  }

  va_start(args, error_id);

  LogEvent()
      .prio(lvl)
      .errcode(error_id)
      .subsys(component)
      .component(component)
      .source_line(line)
      .source_file(file)
      .lookup_quotedv(error_id, "Component reported", args);
  va_end(args);
  return false;
}
