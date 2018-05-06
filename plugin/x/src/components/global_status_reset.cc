/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include "plugin/x/src/services/mysqlx_maintenance.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysqlx_maintenance);

namespace {

long long reset_global_status_variables(UDF_INIT *, UDF_ARGS *, char *,
                                        char *) {
  return mysql_service_mysqlx_maintenance->reset_global_status_variables() ? 1
                                                                           : 0;
}

const char *const udf_name = "mysqlx_reset_global_status_variables";

mysql_service_status_t udf_register() {
  bool ret_int = mysql_service_udf_registration->udf_register(
      udf_name, INT_RESULT,
      reinterpret_cast<Udf_func_any>(reset_global_status_variables), nullptr,
      nullptr);
  return ret_int;
}

mysql_service_status_t udf_unregister() {
  int was_present = 0;
  mysql_service_udf_registration->udf_unregister(udf_name, &was_present);
  return was_present ? 0 : 1;
}

BEGIN_COMPONENT_PROVIDES(mysqlx_global_status_reset)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(mysqlx_global_status_reset)
REQUIRES_SERVICE(udf_registration), REQUIRES_SERVICE(mysqlx_maintenance),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(mysqlx_global_status_reset)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(mysqlx_global_status_reset, udf_name)
udf_register, udf_unregister END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(mysqlx_global_status_reset)
    END_DECLARE_LIBRARY_COMPONENTS

}  // namespace
