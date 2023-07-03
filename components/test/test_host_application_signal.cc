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

#include <assert.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/services/my_host_application_signal.h>
#include <mysql/components/services/udf_registration.h>
#include <stdio.h>

REQUIRES_SERVICE_PLACEHOLDER(host_application_signal);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

BEGIN_COMPONENT_PROVIDES(test_host_application_signal)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_host_application_signal)
REQUIRES_SERVICE(host_application_signal), REQUIRES_SERVICE(udf_registration),
    END_COMPONENT_REQUIRES();

static long long test_shutdown_signal_udf(UDF_INIT *, UDF_ARGS *args,
                                          unsigned char *,
                                          unsigned char *error) {
  if (args->arg_count > 0 && args->arg_type[0] == INT_RESULT) {
    switch (*(reinterpret_cast<long long *>(args->args[0]))) {
      case 1:
        my_host_application_signal_shutdown(mysql_service_registry);
        break;
      case 0:
        mysql_service_host_application_signal->signal(
            HOST_APPLICATION_SIGNAL_SHUTDOWN, nullptr);
        break;
      case 2:
        mysql_service_host_application_signal->signal(
            HOST_APPLICATION_SIGNAL_LAST, nullptr);
        break;
    }
  } else
    *error = 1;
  return 0;
}

static mysql_service_status_t init() {
  if (mysql_service_udf_registration->udf_register(
          "test_shutdown_signal", INT_RESULT,
          reinterpret_cast<Udf_func_any>(test_shutdown_signal_udf), nullptr,
          nullptr)) {
    fprintf(stderr, "Can't register the test_shutdown_signal UDF\n");
    return 1;
  }

  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister("test_shutdown_signal",
                                                     &was_present))
    fprintf(stderr, "Can't unregister the test_shutdown_signal UDF\n");
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_host_application_signal)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_host_application_signal,
                  "mysql:test_host_application_signal")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_host_application_signal)
    END_DECLARE_LIBRARY_COMPONENTS
