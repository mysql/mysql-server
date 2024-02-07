/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include <fcntl.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/plugin.h>

#include "my_macros.h"
#include "nulls.h"
#include "template_utils.h"
#include "typelib.h"

#define VARIABLE_BUFFER_SIZE 1023
#define MAX_BUFFER_LENGTH 100
int log_text_len = 0;
char log_text[MAX_BUFFER_LENGTH];
FILE *outfile;
const char *filename = "test_component_sys_var_service_same.log";

#define WRITE_LOG(format, lit_log_text)                                 \
  log_text_len = sprintf(log_text, format, lit_log_text);               \
  if (fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile) != \
      static_cast<size_t>(log_text_len))                                \
    return true;

REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);

/**
  This file contains a test (example) component, which tests the services of
  "component_sys_variable_register" and component_sys_variable_unregister,
  provided by the component "mysql_server" component.
*/

enum password_policy_enum {
  PASSWORD_POLICY_LOW,
  PASSWORD_POLICY_MEDIUM,
  PASSWORD_POLICY_STRONG
};

static const char *policy_names[] = {"LOW", "MEDIUM", "STRONG", NullS};

static TYPE_LIB password_policy_typelib_t = {array_elements(policy_names) - 1,
                                             "password_policy_typelib_t",
                                             policy_names, nullptr};
static ulong enum_variable_value;
static char *str_variable_value;
static int int_variable_value;
static int uint_variable_value;
static long long_variable_value;
static ulong ulong_variable_value;
static longlong longlong_variable_value;
static ulonglong ulonglong_variable_value;
static bool bool_variable_value;

/**
  Initialization entry method for test component. It executes the tests of
  the service.
*/
static mysql_service_status_t test_component_sys_var_service_same_init() {
  char *var_value;
  size_t len;

  unlink(filename);
  outfile = fopen(filename, "w+");

  WRITE_LOG("%s\n", "test_component_sys_var_same init:");

  var_value = new char[VARIABLE_BUFFER_SIZE + 1];

  INTEGRAL_CHECK_ARG(int) int_arg;
  int_arg.def_val = 8;
  int_arg.min_val = 0;
  int_arg.max_val = 1024;
  int_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "int_sys_var", PLUGIN_VAR_INT,
          "Registering int sys_variable", nullptr, nullptr, (void *)&int_arg,
          (void *)&int_variable_value)) {
    WRITE_LOG("%s\n", "int register_variable failed.");
  }

  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "int_sys_var", PLUGIN_VAR_INT,
          "Registering int sys_variable", nullptr, nullptr, (void *)&int_arg,
          (void *)&int_variable_value)) {
    WRITE_LOG("%s\n", "int register_variable failed.");
  }

  INTEGRAL_CHECK_ARG(uint) uint_arg;
  uint_arg.def_val = 1024;
  uint_arg.min_val = 10;
  uint_arg.max_val = 10241024;
  uint_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "uint_sys_var",
          PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED, "Registering uint sys_variable",
          nullptr, nullptr, (void *)&uint_arg, (void *)&uint_variable_value)) {
    WRITE_LOG("%s\n", "uint register_variable failed.");
  }

  INTEGRAL_CHECK_ARG(long) long_arg;
  long_arg.def_val = 100;
  long_arg.min_val = 10;
  long_arg.max_val = 100;
  long_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "long_sys_var", PLUGIN_VAR_LONG,
          "Registering long sys_variable", nullptr, nullptr, (void *)&long_arg,
          (void *)&long_variable_value)) {
    WRITE_LOG("%s\n", "long register_variable failed.");
  }

  INTEGRAL_CHECK_ARG(ulong) ulong_arg;
  ulong_arg.def_val = 8192;
  ulong_arg.min_val = 1000;
  ulong_arg.max_val = 81928192;
  ulong_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "ulong_sys_var",
          PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED,
          "Registering unsigned long sys_variable", nullptr, nullptr,
          (void *)&ulong_arg, (void *)&ulong_variable_value)) {
    WRITE_LOG("%s\n", "ulong register_variable failed.");
  }

  INTEGRAL_CHECK_ARG(longlong) longlong_arg;
  longlong_arg.def_val = 8192;
  longlong_arg.min_val = 1000;
  longlong_arg.max_val = 8192819281928192;
  longlong_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "longlong_sys_var", PLUGIN_VAR_LONGLONG,
          "Registering longlong sys_variable", nullptr, nullptr,
          (void *)&longlong_arg, (void *)&longlong_variable_value)) {
    WRITE_LOG("%s\n", "longlong register_variable failed.");
  }

  INTEGRAL_CHECK_ARG(ulonglong) ulonglong_arg;
  ulonglong_arg.def_val = 8192;
  ulonglong_arg.min_val = 1000;
  ulonglong_arg.max_val = 8192819281928192;
  ulonglong_arg.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "ulonglong_sys_var",
          PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED,
          "Registering unsigned longlong sys_variable", nullptr, nullptr,
          (void *)&ulonglong_arg, (void *)&ulonglong_variable_value)) {
    WRITE_LOG("%s\n", "unsigned longlong register_variable failed.");
  }

  BOOL_CHECK_ARG(bool) bool_arg;
  bool_arg.def_val = true;

  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "bool_sys_var", PLUGIN_VAR_BOOL,
          "Registering bool sys_variable", nullptr, nullptr, (void *)&bool_arg,
          (void *)&bool_variable_value)) {
    WRITE_LOG("%s\n", "register_variable failed.");
  }

  ENUM_CHECK_ARG(enum) enum_arg;
  enum_arg.def_val = PASSWORD_POLICY_MEDIUM;
  enum_arg.typelib = &password_policy_typelib_t;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "enum_sys_var", PLUGIN_VAR_ENUM,
          "Registering enum sys_variable", nullptr, nullptr, (void *)&enum_arg,
          (void *)&enum_variable_value)) {
    WRITE_LOG("%s\n", "register_variable failed.");
  }

  STR_CHECK_ARG(str) str_arg;
  str_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "str_sys_var", PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "Registering string sys_variable", nullptr, nullptr, (void *)&str_arg,
          (void *)&str_variable_value)) {
    WRITE_LOG("%s\n", "register_variable failed.");
  }

  len = VARIABLE_BUFFER_SIZE;
  if (mysql_service_component_sys_variable_register->get_variable(
          "test_component", "int_sys_var", (void **)&var_value, &len)) {
    WRITE_LOG("%s\n", "get_variable failed.");
  } else {
    WRITE_LOG("variable value : %s\n", var_value);
  }

  delete[] var_value;
  WRITE_LOG("%s\n", "End of init");
  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_component_sys_var_service_same_deinit() {
  outfile = fopen(filename, "a+");
  WRITE_LOG("%s\n", "test_component_sys_var_same deinit:");

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "int_sys_var")) {
    WRITE_LOG("%s\n", "int unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "uint_sys_var")) {
    WRITE_LOG("%s\n", "unsigned int unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "long_sys_var")) {
    WRITE_LOG("%s\n", "long unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "ulong_sys_var")) {
    WRITE_LOG("%s\n", "unsigned long unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "longlong_sys_var")) {
    WRITE_LOG("%s\n", "longlong unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "ulonglong_sys_var")) {
    WRITE_LOG("%s\n", "unsigned longlong unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "bool_sys_var")) {
    WRITE_LOG("%s\n", "unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "enum_sys_var")) {
    WRITE_LOG("%s\n", "unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "str_sys_var")) {
    WRITE_LOG("%s\n", "unregister_variable failed.");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component", "str_sys_var")) {
    WRITE_LOG("%s\n", "unregister_variable failed.");
  }

  fclose(outfile);
  return false;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_component_sys_var_service)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_component_sys_var_service)
REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_sys_var_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_component_sys_var_service", "1"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_sys_var_service,
                  "mysql:test_component_sys_var_service")
test_component_sys_var_service_same_init,
    test_component_sys_var_service_same_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_sys_var_service)
    END_DECLARE_LIBRARY_COMPONENTS
