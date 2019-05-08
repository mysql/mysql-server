/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/x/src/udf/mysqlx_get_prepared_statement_id.h"

#include <cstring>
#include "include/my_sys.h"
#include "include/mysql/thread_pool_priv.h"
#include "plugin/x/src/xpl_server.h"

namespace xpl {

namespace {
bool mysqlx_get_prepared_statement_id_init(UDF_INIT *, UDF_ARGS *args,
                                           char *message) {
  if (args->arg_count == 1 && args->arg_type[0] == INT_RESULT) return false;

  sprintf(message, "Function expect only one numeric argument");
  return true;
}

long long mysqlx_get_prepared_statement_id(UDF_INIT *, UDF_ARGS *args,
                                           unsigned char *is_null,
                                           unsigned char *error) {
  *error = 0;
  uint32_t stmt_id = 0;
  if (xpl::Server::get_prepared_statement_id(
          thd_get_current_thd(), *reinterpret_cast<long long *>(args->args[0]),
          &stmt_id)) {
    *is_null = 0;
    return stmt_id;
  }
  *is_null = 1;
  return 0;
}
}  // namespace

namespace udf {
Registrator::Record get_mysqlx_get_prepared_statement_id_record() {
  return {
      "mysqlx_get_prepared_statement_id",
      INT_RESULT,
      reinterpret_cast<Udf_func_any>(mysqlx_get_prepared_statement_id),
      mysqlx_get_prepared_statement_id_init,
      nullptr,
  };
}
}  // namespace udf
}  // namespace xpl
