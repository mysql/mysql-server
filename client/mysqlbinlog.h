/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQLBINLOG_INCLUDED
#define MYSQLBINLOG_INCLUDED

#include "my_global.h"

class Format_description_log_event;

extern my_bool force_opt;
extern my_bool short_form;
extern ulong opt_server_id_mask;
extern ulong opt_binlog_rows_event_max_size;

extern Format_description_log_event* glob_description_event;

/*
  error() is used in macro BINLOG_ERROR which is invoked in
  rpl_gtid.h, hence the early forward declaration.
*/
void error(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
void warning(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
void error_or_warning(const char *format, va_list args, const char *msg)
  __attribute__((format(printf, 1, 0)));
void sql_print_error(const char *format,...)
  __attribute__((format(printf, 1, 2)));

#endif  // MYSQLBINLOG_INCLUDED
