/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "plugin/x/src/udf/mysqlx_error.h"

#include <cstring>

#include "my_sys.h"  // NOLINT(build/include_subdir)

namespace xpl {

namespace {
bool mysqlx_error_init(UDF_INIT *, UDF_ARGS *args, char *message) {
  if (args->arg_count == 1 && args->arg_type[0] == INT_RESULT) return false;

  sprintf(message, "Function expect only one numeric argument");
  return true;
}

// NOLINTNEXTLINE(runtime/int)
char *mysqlx_error(UDF_INIT *, UDF_ARGS *args, char *, unsigned long *,
                   unsigned char *is_null, unsigned char *error) {
  // NOLINTNEXTLINE(runtime/int)
  my_message(*reinterpret_cast<long long *>(args->args[0]),
             "Mysqlx internal error", MYF(0));
  *error = 1;
  *is_null = 1;
  return nullptr;
}
}  // namespace

namespace udf {
Registrator::Record get_mysqlx_error_record() {
  return {
      "mysqlx_error",
      STRING_RESULT,
      reinterpret_cast<Udf_func_any>(mysqlx_error),
      mysqlx_error_init,
      nullptr,
  };
}
}  // namespace udf
}  // namespace xpl
