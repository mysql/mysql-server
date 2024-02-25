/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/mysql_lex_string.h>
#include <stdio.h>
#include <string.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

BEGIN_COMPONENT_PROVIDES(test_mysql_current_thread_reader)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_mysql_current_thread_reader)
REQUIRES_SERVICE(mysql_current_thread_reader),
    REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_thd_security_context),
    REQUIRES_SERVICE(mysql_security_context_options), END_COMPONENT_REQUIRES();

static char *test_thd_reader_current_user_udf(UDF_INIT *, UDF_ARGS *args,
                                              char *result,
                                              unsigned long *length,
                                              unsigned char *,
                                              unsigned char *error) {
  if (args->arg_count != 0) {
    *error = 1;
    return nullptr;
  }

  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return nullptr;
  }

  Security_context_handle ctx;
  if (mysql_service_mysql_thd_security_context->get(thd, &ctx)) {
    *error = 1;
    return nullptr;
  }

  MYSQL_LEX_CSTRING user;
  if (mysql_service_mysql_security_context_options->get(ctx, "priv_user",
                                                        &user)) {
    *error = 1;
    return nullptr;
  }

  MYSQL_LEX_CSTRING host;
  if (mysql_service_mysql_security_context_options->get(ctx, "priv_host",
                                                        &host)) {
    *error = 1;
    return nullptr;
  }

  snprintf(reinterpret_cast<char *>(result), 255, "%.*s@%.*s",
           static_cast<int>(user.length), user.str,
           static_cast<int>(host.length), host.str);
  *length = strlen(reinterpret_cast<char *>(result));

  return reinterpret_cast<char *>(result);
}

static mysql_service_status_t init() {
  Udf_func_string udf = test_thd_reader_current_user_udf;
  if (mysql_service_udf_registration->udf_register(
          "test_thd_reader_current_user", STRING_RESULT,
          reinterpret_cast<Udf_func_any>(udf), nullptr, nullptr)) {
    fprintf(stderr, "Can't register the test_thd_reader_current_user UDF\n");
    return 1;
  }

  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister(
          "test_thd_reader_current_user", &was_present))
    fprintf(stderr, "Can't unregister the test_thd_reader_current_user UDF\n");
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_mysql_current_thread_reader)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_current_thread_reader,
                  "mysql:test_mysql_current_thread_reader")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_current_thread_reader)
    END_DECLARE_LIBRARY_COMPONENTS
