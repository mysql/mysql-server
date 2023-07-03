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

#include <mysql/components/services/udf_metadata.h>
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/udf_registration.h"
#include "mysql/plugin.h"
#include "mysql/udf_registration_types.h"

#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/udf_service_util.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
std::string Udf_charset_service::m_service_name{"mysql_udf_metadata"};
std::string Udf_charset_service::m_arg_type{"charset"};
std::string Udf_charset_service::m_charset_name{"latin1"};

bool Udf_charset_service::init() {
  DBUG_TRACE;

  my_h_service h_udf_metadata_service;
  if (!srv_registry ||
      srv_registry->acquire(m_service_name.c_str(), &h_udf_metadata_service)) {
    LogErr(ERROR_LEVEL, ER_UDF_REGISTER_SERVICE_ERROR);
    return true;
  }

  mysql_service_mysql_udf_metadata =
      reinterpret_cast<SERVICE_TYPE(mysql_udf_metadata) *>(
          h_udf_metadata_service);
  assert(mysql_service_mysql_udf_metadata != nullptr);

  return false;
}

bool Udf_charset_service::deinit() {
  DBUG_TRACE;

  using udf_metadata_t = SERVICE_TYPE_NO_CONST(mysql_udf_metadata);
  if (!srv_registry || !mysql_service_mysql_udf_metadata ||
      srv_registry->release(reinterpret_cast<my_h_service>(
          const_cast<udf_metadata_t *>(mysql_service_mysql_udf_metadata)))) {
    LogErr(ERROR_LEVEL, ER_UDF_UNREGISTER_ERROR);
    return true;
  }

  return false;
}

bool Udf_charset_service::set_return_value_charset(UDF_INIT *initid) {
  DBUG_TRACE;
  char charset_name[MAX_CHARSET_LEN];

  if (init()) return true;
  m_charset_name.copy(charset_name, m_charset_name.size() + 1);
  charset_name[m_charset_name.size()] = '\0';

  if (mysql_service_mysql_udf_metadata->result_set(
          initid, Udf_charset_service::m_arg_type.c_str(),
          static_cast<void *>(charset_name))) {
    deinit();
    return true;
  }

  return deinit();
}

bool Udf_charset_service::set_args_charset(UDF_ARGS *args) {
  DBUG_TRACE;
  char charset_name[MAX_CHARSET_LEN];

  init();
  m_charset_name.copy(charset_name, m_charset_name.size() + 1);
  charset_name[m_charset_name.size()] = '\0';

  for (uint index = 0; index < args->arg_count; ++index) {
    if (args->arg_type[index] == STRING_RESULT &&
        mysql_service_mysql_udf_metadata->argument_set(
            args, Udf_charset_service::m_arg_type.c_str(), index,
            static_cast<void *>(charset_name))) {
      deinit();
      return true;
    }
  }

  return deinit();
}
