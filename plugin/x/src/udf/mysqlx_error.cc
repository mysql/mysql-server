/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/x/src/udf/mysqlx_error.h"

#include <cstring>
#include "include/my_sys.h"

namespace xpl {

namespace {
bool mysqlx_error_init(UDF_INIT *, UDF_ARGS *args, char *message) {
  if (args->arg_count == 1 && args->arg_type[0] == INT_RESULT) return false;

  sprintf(message, "Function expect only one numeric argument");
  return true;
}

char *mysqlx_error(UDF_INIT *, UDF_ARGS *args, char *, unsigned long *, char *,
                   char *error) {
  my_message(*reinterpret_cast<long long *>(args->args[0]),
             "Mysqlx internal error", MYF(0));
  *error = 1;
  return nullptr;
}
}  // namespace

namespace udf {
Registrator::Record get_mysqlx_error_record() {
  return {"mysqlx_error", STRING_RESULT,
          reinterpret_cast<Udf_func_any>(mysqlx_error),
          reinterpret_cast<Udf_func_init>(mysqlx_error_init), nullptr};
}
}  // namespace udf
}  // namespace xpl
