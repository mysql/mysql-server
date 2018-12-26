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

/**
  This file contains a test (example) component, which tests the apis of
  "status_variable_registration" service provided by the
  "mysql_server" component.
*/

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define MAX_BUFFER_LENGTH 100
int log_text_len = 0;
char log_text[MAX_BUFFER_LENGTH];
FILE *outfile;
const char *filename = "test_component_status_var_service.log";
class THD;

#define WRITE_LOG(format, lit_log_text)                   \
  log_text_len = sprintf(log_text, format, lit_log_text); \
  fwrite((unsigned char *)log_text, sizeof(char), log_text_len, outfile)

REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);

static int int_variable_value = 7;
static bool bool_variable_value = true;
static long long_variable_value = 1234567;
static double double_variable_value = 8.0;
static long long longlong_variable_value = 123456789;
static char char_variable_value[] = "Testing CHAR status variable";
static char *char_ptr_variable_value = nullptr;

using std::swap;
static void char_ptr_foo() {
  char char_buff[50];
  char *char_ptr;

  snprintf(char_buff, sizeof(char_buff), "Testing CHAR_PTR for status vars");
  char_ptr = strdup(char_buff);
  swap(char_ptr, char_ptr_variable_value);

  if (char_ptr) free(char_ptr);
  return;
}

/*
  Constructing the status variable objects for different types, used to pass
  as a parameter to register_variable()/unregister_variable() apis.
*/
static SHOW_VAR status_int_var[] = {
    {"test_component.int_var", (char *)&int_variable_value, SHOW_INT,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_bool_var[] = {
    {"test_component.bool_var", (char *)&bool_variable_value, SHOW_BOOL,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_long_var[] = {
    {"test_component.long_var", (char *)&long_variable_value, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_longlong_var[] = {
    {"test_component.longlong_var", (char *)&longlong_variable_value,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_char_var[] = {
    {"test_component.char_var", (char *)&char_variable_value, SHOW_CHAR,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_char_ptr_var[] = {
    {"test_component.char_ptr_var", (char *)&char_ptr_variable_value,
     SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_double_var[] = {
    {"test_component.double_var", (char *)&double_variable_value, SHOW_DOUBLE,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};

static int show_func_example(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  snprintf(buf, 100, "Testing SHOW_FUNC type for component status variables");
  return 0;
}
static SHOW_VAR status_func_var[] = {
    {"test_component.func_var", (char *)show_func_example, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};

struct example_vars_t {
  char var1[10];
  double var2;
};
example_vars_t example_vars = {"MySQL", 8.0};
static SHOW_VAR array_value[] = {
    {"var1", (char *)&example_vars.var1, SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {"var2", (char *)&example_vars.var2, SHOW_DOUBLE, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};
static SHOW_VAR status_array_var[] = {
    {"test_component.array", (char *)array_value, SHOW_ARRAY,
     SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};

/**
  Initialization entry method for test component. It executes the tests of
  the service.
*/
static mysql_service_status_t test_component_status_var_service_init() {
  outfile = fopen(filename, "w+");
  char_ptr_variable_value = nullptr;

  WRITE_LOG("%s\n", "test_component_status_var init:");
  char_ptr_foo();

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_int_var)) {
    WRITE_LOG("%s\n", "integer register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_bool_var)) {
    WRITE_LOG("%s\n", "bool register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_long_var)) {
    WRITE_LOG("%s\n", "long register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_longlong_var)) {
    WRITE_LOG("%s\n", "longlong register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_char_var)) {
    WRITE_LOG("%s\n", "char register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_char_ptr_var)) {
    WRITE_LOG("%s\n", "char_ptr register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_double_var)) {
    WRITE_LOG("%s\n", "double register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_func_var)) {
    WRITE_LOG("%s\n", "func register_variable failed.");
  }
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&status_array_var)) {
    WRITE_LOG("%s\n", "array register_variable failed.");
  }
  WRITE_LOG("%s\n", "test_component_status_var end of init:");
  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_component_status_var_service_deinit() {
  outfile = fopen(filename, "a+");

  WRITE_LOG("%s\n", "test_component_status_var deinit:");

  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_int_var)) {
    WRITE_LOG("%s\n", "integer unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_bool_var)) {
    WRITE_LOG("%s\n", "bool unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_long_var)) {
    WRITE_LOG("%s\n", "long unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_longlong_var)) {
    WRITE_LOG("%s\n", "longlong unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_char_var)) {
    WRITE_LOG("%s\n", "char unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_char_ptr_var)) {
    WRITE_LOG("%s\n", "char_ptr unregister_variable failed.");
  }
  free(char_ptr_variable_value);
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_double_var)) {
    WRITE_LOG("%s\n", "double unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_func_var)) {
    WRITE_LOG("%s\n", "func unregister_variable failed.");
  }
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&status_array_var)) {
    WRITE_LOG("%s\n", "array unregister_variable failed.");
  }
  WRITE_LOG("%s\n", "test_component_status_var_service end of deinit:");

  fclose(outfile);
  return false;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_component_status_var_service)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_component_status_var_service)
REQUIRES_SERVICE(status_variable_registration), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_status_var_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_component_status_var_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_status_var_service,
                  "mysql:test_component_status_var_service")
test_component_status_var_service_init,
    test_component_status_var_service_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_status_var_service)
    END_DECLARE_LIBRARY_COMPONENTS
