/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
#include <stdio.h>
#include <string.h>

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_update_string);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);

BEGIN_COMPONENT_PROVIDES(test_mysql_system_variable_set)
END_COMPONENT_PROVIDES();

// clang-format off
BEGIN_COMPONENT_REQUIRES(test_mysql_system_variable_set)
  REQUIRES_SERVICE(mysql_current_thread_reader),
  REQUIRES_SERVICE(mysql_system_variable_update_string),
  REQUIRES_SERVICE(udf_registration),
  REQUIRES_SERVICE(mysql_string_factory),
  REQUIRES_SERVICE(mysql_string_converter),
  REQUIRES_SERVICE(mysql_udf_metadata),
END_COMPONENT_REQUIRES();
// clang-format on

bool test_set_system_variable_string_init(UDF_INIT *, UDF_ARGS *args,
                                          char *message) {
  if (args->arg_count != 4) {
    strcpy(message, "wrong number of arguments");
    return true;
  }

  args->maybe_null[0] = false;
  args->maybe_null[1] = false;
  args->maybe_null[2] = false;
  args->maybe_null[3] = false;

  if (args->arg_type[0] != INT_RESULT) {
    strcpy(message, "The first argument must be an int");
    return true;
  }
  if (args->arg_type[1] != STRING_RESULT) {
    strcpy(message, "The second argument must be a string");
    return true;
  }
  if (args->arg_type[2] != STRING_RESULT) {
    strcpy(message, "The third argument must be a string");
    return true;
  }
  if (args->arg_type[3] != STRING_RESULT) {
    strcpy(message, "The forth argument must be a string");
    return true;
  }

  void *cset = (void *)const_cast<char *>("latin1");
  if (mysql_service_mysql_udf_metadata->argument_set(args, "charset", 3,
                                                     cset)) {
    strcpy(message,
           "Failed to set latin1 as character set for the forth argument");
    return true;
  }
  return false;
}

long long test_set_system_variable_string(UDF_INIT * /*initd*/, UDF_ARGS *args,
                                          unsigned char * /*is_null*/,
                                          unsigned char *error) {
  bool make_new_thread = *((long long *)args->args[0]) > 0;

  MYSQL_THD thd = nullptr;

  *error = 0;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  void *arg1_cs, *arg2_cs;
  if (mysql_service_mysql_udf_metadata->argument_get(args, "charset", 1,
                                                     &arg1_cs) ||
      mysql_service_mysql_udf_metadata->argument_get(args, "charset", 2,
                                                     &arg2_cs)) {
    *error = 1;
    return 0;
  }

  my_h_string name = nullptr, value = nullptr;
  if (mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[1], args->lengths[1], ((const char *)arg1_cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &value, args->args[2], args->lengths[2], ((const char *)arg2_cs))) {
    if (name) mysql_service_mysql_string_factory->destroy(name);
    if (value) mysql_service_mysql_string_factory->destroy(value);
    *error = 1;
    return 0;
  }

  if (mysql_service_mysql_system_variable_update_string->set(
          thd, args->args[3], nullptr, name, value))
    *error = 1;

  if (name) mysql_service_mysql_string_factory->destroy(name);
  if (value) mysql_service_mysql_string_factory->destroy(value);
  return *error ? 1 : 0;
}

static mysql_service_status_t init() {
  Udf_func_longlong udf = test_set_system_variable_string;
  if (mysql_service_udf_registration->udf_register(
          "test_set_system_variable_string", INT_RESULT,
          reinterpret_cast<Udf_func_any>(udf),
          test_set_system_variable_string_init, nullptr)) {
    fprintf(stderr, "Can't register the test_set_system_variable_string UDF\n");
    return 1;
  }

  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (mysql_service_udf_registration->udf_unregister(
          "test_set_system_variable_string", &was_present))
    fprintf(stderr,
            "Can't unregister the test_set_system_variable_string UDF\n");
  return 0; /* success */
}

// clang-format off
BEGIN_COMPONENT_METADATA(test_mysql_system_variable_set)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_system_variable_set,
                  "mysql:test_mysql_system_variable_set")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_system_variable_set)
    END_DECLARE_LIBRARY_COMPONENTS
    // clang-format on
