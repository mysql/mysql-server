/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "my_sys.h"  // NOLINT(build/include_subdir)

#include "mysql/thread_pool_priv.h"
#include "plugin/x/src/module_mysqlx.h"

namespace xpl {
namespace {

bool get_prepared_statement_id(const THD *thd, const uint32_t client_stmt_id,
                               uint32_t *out_stmt_id) {
  auto server = modules::Module_mysqlx::get_instance_server();

  if (nullptr == server.container()) return false;

  auto client = server->get_client(thd);

  if (!client) return false;

  auto session = client->session_shared_ptr();

  if (!session) return false;

  return session->get_prepared_statement_id(client_stmt_id, out_stmt_id);
}

bool mysqlx_get_prepared_statement_id_init(UDF_INIT *, UDF_ARGS *args,
                                           char *message) {
  if (args->arg_count == 1 && args->arg_type[0] == INT_RESULT) return false;

  sprintf(message, "Function expect only one numeric argument");
  return true;
}

// NOLINTNEXTLINE(runtime/int)
long long mysqlx_get_prepared_statement_id(UDF_INIT *, UDF_ARGS *args,
                                           unsigned char *is_null,
                                           unsigned char *error) {
  *error = 0;
  uint32_t stmt_id = 0;
  if (get_prepared_statement_id(thd_get_current_thd(),
                                // NOLINTNEXTLINE(runtime/int)
                                *reinterpret_cast<long long *>(args->args[0]),
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
