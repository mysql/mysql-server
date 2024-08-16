/* Copyright (c) 2023, 2024 Oracle and/or its affiliates.

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

#include "server_logs_helpers.h"

bool parse_log_level(const char *level, OTELLogLevel &result) {
  if (0 == strcmp(level, "error"))
    result = TLOG_ERROR;
  else if (0 == strcmp(level, "warn"))
    result = TLOG_WARN;
  else if (0 == strcmp(level, "info"))
    result = TLOG_INFO;
  else if (0 == strcmp(level, "debug"))
    result = TLOG_DEBUG;
  else
    return true;
  return false;  // success
}

const char *print_log_level(OTELLogLevel severity) {
  switch (severity) {
    case TLOG_ERROR:
      return "error";
    case TLOG_WARN:
      return "warn";
    case TLOG_INFO:
      return "info";
    case TLOG_DEBUG:
      return "debug";
    default:
      return "none";
  }
}
