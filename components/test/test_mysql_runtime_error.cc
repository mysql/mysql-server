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

#include <assert.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <mysqld_error.h>
#include <stdio.h>

namespace mysql_runtime_error {  // To avoid ODR asan error
#include <mysql/components/services/mysql_runtime_error_service.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

BEGIN_COMPONENT_PROVIDES(test_mysql_runtime_error)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_mysql_runtime_error)
REQUIRES_SERVICE(mysql_runtime_error), REQUIRES_SERVICE(udf_registration),
    END_COMPONENT_REQUIRES();

static long long test_mysql_runtime_error_udf(UDF_INIT *, UDF_ARGS *args,
                                              unsigned char *,
                                              unsigned char *error) {
  if (args->arg_count > 0 && args->arg_type[0] == INT_RESULT) {
    switch (*(reinterpret_cast<long long *>(args->args[0]))) {
      case 0:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_emit_printf(
            mysql_service_mysql_runtime_error, ER_COMPONENTS_UNLOAD_NOT_LOADED,
            0,
            "This is to test the mysql_runtime_error service"
            " using utility function");
        break;
      case 1:
        /* Checking mysql_runtime_error service, similar signature as my_error
           api. This need REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error)
           definition. */
        mysql_error_service_printf(
            ER_COMPONENTS_UNLOAD_NOT_LOADED, 0,
            "This is to test the mysql_runtime_error service");
        break;
      case 3:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_emit_printf(
            mysql_service_mysql_runtime_error,
            ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE, 0,
            "This is to test the mysql_runtime_error service",
            " using utility function");
        break;
      case 4:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_emit_printf(mysql_service_mysql_runtime_error,
                                        ER_INVALID_THREAD_PRIORITY, 0, 123,
                                        "Test", "Test group", 0, 99);
        break;
      case 5:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_emit_printf(mysql_service_mysql_runtime_error,
                                        ER_REGEXP_TIME_OUT, 0);
        break;
      case 6:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_emit_printf(mysql_service_mysql_runtime_error,
                                        ER_TOO_LONG_KEY, 0, 1024);
        break;
      case 7:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_printf(
            ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE, 0,
            "This is to test the mysql_runtime_error service",
            " using utility function");
        break;
      case 8:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_printf(ER_INVALID_THREAD_PRIORITY, 0, 123, "Test",
                                   "Test group", 0, 99);
        break;
      case 9:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_printf(ER_REGEXP_TIME_OUT, 0);
        break;
      case 10:
        /* Checking mysql_runtime_error service through utility api */
        mysql_error_service_printf(ER_TOO_LONG_KEY, 0, 1024);
        break;
      case 2:
        /* default mysql_runtime_error service, This can be checked with
           minimal_chassis work log(wl#11080) */
        break;
    }
  } else
    *error = 1;
  return 0;
}

static mysql_service_status_t init() {
  if (mysql_service_udf_registration->udf_register(
          "test_mysql_runtime_error", INT_RESULT,
          reinterpret_cast<Udf_func_any>(test_mysql_runtime_error_udf), nullptr,
          nullptr)) {
    fprintf(stderr, "Can't register the test_mysql_runtime_error UDF\n");
    return 1;
  }
  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister("test_mysql_runtime_error",
                                                     &was_present)) {
    fprintf(stderr, "Can't unregister the test_mysql_runtime_error UDF\n");
    return 1;
  }
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_mysql_runtime_error)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_runtime_error, "mysql:test_mysql_runtime_error")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_runtime_error)
    END_DECLARE_LIBRARY_COMPONENTS

}  // namespace mysql_runtime_error
