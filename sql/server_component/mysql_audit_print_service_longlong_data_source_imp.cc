/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mysql_audit_print_service_longlong_data_source_imp.h"
#include <sql/events.h>
#include <sql/sql_class.h>

#define EQUALS(X)                                                    \
  !sortcmp_lex_string(base->lex_cstring(), {(X), ((sizeof(X) - 1))}, \
                      base->charset())

DEFINE_BOOL_METHOD(mysql_audit_print_service_longlong_data_source_imp::get,
                   (MYSQL_THD thd, my_h_string name, long long *out)) {
  *out = 0;

  String *base = reinterpret_cast<String *>(name);

  if (!base || !base->charset()) return true;

  if (EQUALS("query_time")) {
    if (!thd->start_utime) return true;
    ulonglong current_utime = my_micro_time();
    ulonglong query_utime = (current_utime - thd->start_utime);
    *out = static_cast<long long>(query_utime);
  } else if (EQUALS("rows_sent")) {
    *out = static_cast<long long>(thd->get_sent_row_count());
  } else if (EQUALS("rows_examined")) {
    *out = static_cast<long long>(thd->get_examined_row_count());
  } else if (EQUALS("bytes_received")) {
    if (!thd->copy_status_var_ptr) return true;
    *out = static_cast<long long>(thd->status_var.bytes_received -
                                  thd->copy_status_var_ptr->bytes_received);
  } else if (EQUALS("bytes_sent")) {
    if (!thd->copy_status_var_ptr) return true;
    *out = static_cast<long long>(thd->status_var.bytes_sent -
                                  thd->copy_status_var_ptr->bytes_sent);
  } else
    return true;

  return false;
}
