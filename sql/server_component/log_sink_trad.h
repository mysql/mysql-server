/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file log_sink_trad.h

  Functions in the built-in log-sink (i.e. the writer for the traditional
  MySQL error log):

  a) writing an error log event to the traditional error log file
  b) parsing a line from the traditional error log file
*/

#ifndef LOG_SINK_TRAD_H
#define LOG_SINK_TRAD_H

#include "log_builtins_internal.h"
#include "my_compiler.h"

ssize_t parse_trad_field(const char *parse_from, const char **token_end,
                         const char *buf_end);

log_service_error log_sink_trad_parse_log_line(const char *line_start,
                                               size_t line_length);

int log_sink_trad(void *instance [[maybe_unused]], log_line *ll);

#endif /* LOG_SINK_BUFFER_H */
