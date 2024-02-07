/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/x/src/udf/mysqlx_generate_document_id.h"

#include <cstring>
#include <string>

#include "my_sys.h"  // NOLINT(build/include_subdir)
#include "mysql/thread_pool_priv.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/variables/system_variables.h"

namespace xpl {
namespace {

bool mysqlx_generate_document_id_init(UDF_INIT *, UDF_ARGS *args,
                                      char *message) {
  switch (args->arg_count) {
    case 0:
      return false;
    case 1:
      if (args->arg_type[0] == INT_RESULT) return false;
      strcpy(message, "Function expect integer argument");
      return true;
    case 2:
      if (args->arg_type[0] == INT_RESULT && args->arg_type[1] == INT_RESULT)
        return false;
      strcpy(message, "Function expect two integer arguments");
      return true;
    case 3:
      if (args->arg_type[0] == INT_RESULT && args->arg_type[1] == INT_RESULT &&
          args->arg_type[2] == INT_RESULT)
        return false;
      strcpy(message, "Function expect three integer arguments");
      return true;
    default:
      strcpy(message, "Function expect up to three integer arguments");
  }
  return true;
}

std::string get_document_id(const THD *thd, const uint16_t offset,
                            const uint16_t increment) {
  using Variables = iface::Document_id_generator::Variables;
  Variables vars{static_cast<uint16_t>(
                     xpl::Plugin_system_variables::m_document_id_unique_prefix),
                 offset, increment};

  auto server = modules::Module_mysqlx::get_instance_server();

  if (server.container()) {
    auto client = server->get_client(thd);

    if (client) {
      auto session = client->session_shared_ptr();

      if (session) {
        return session->get_document_id_aggregator().generate_id(vars);
      }
    }

    return server->get_document_id_generator().generate(vars);
  }

  return "";
}

char *mysqlx_generate_document_id(UDF_INIT *, UDF_ARGS *args, char *result,
                                  unsigned long *length, unsigned char *is_null,
                                  unsigned char *error) {
  uint16_t offset{1}, increment{1};
  switch (args->arg_count) {
    case 3:
      if (*reinterpret_cast<long long *>(args->args[2])) {
        *is_null = 1;
        return nullptr;
      }
      [[fallthrough]];
    case 2:
      increment = *reinterpret_cast<long long *>(args->args[1]);
      [[fallthrough]];
    case 1:
      offset = *reinterpret_cast<long long *>(args->args[0]);
  }

  *error = 0;
  *is_null = 0;
  *length = sprintf(
      result, "%s",
      get_document_id(thd_get_current_thd(), offset, increment).c_str());
  return result;
}

}  // namespace

namespace udf {

Registrator::Record get_mysqlx_generate_document_id_record() {
  return {
      "mysqlx_generate_document_id",
      STRING_RESULT,
      reinterpret_cast<Udf_func_any>(mysqlx_generate_document_id),
      mysqlx_generate_document_id_init,
      nullptr,
  };
}

}  // namespace udf
}  // namespace xpl
