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

/**
  Simplified error logging service
*/

#ifndef MYSQL_SIMPLE_ERROR_LOG_H_
#define MYSQL_SIMPLE_ERROR_LOG_H_

#include "mysql/components/service.h"

// Keep in sync with my_loglevel.h
#define MYSQL_ERROR_LOG_SEVERITY_SYSTEM 0
#define MYSQL_ERROR_LOG_SEVERITY_ERROR 1
#define MYSQL_ERROR_LOG_SEVERITY_WARNING 2
#define MYSQL_ERROR_LOG_SEVERITY_INFORMATION 3

BEGIN_SERVICE_DEFINITION(mysql_simple_error_log)
DECLARE_BOOL_METHOD(emit,
                    (const char *component, const char *file,
                     unsigned long line, int severity, int error_id, ...));
END_SERVICE_DEFINITION(mysql_simple_error_log)

#define mysql_simple_error_log_emit(component, severity, error_id, ...)     \
  mysql_service_mysql_simple_error_log->emit(component, __FILE__, __LINE__, \
                                             severity, error_id, __VA_ARGS__)
#endif /* MYSQL_SIMPLE_ERROR_LOG_H_ */
