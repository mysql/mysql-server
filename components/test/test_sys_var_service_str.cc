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
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/plugin.h>

#include "my_macros.h"
#include "typelib.h"

#define MAX_BUFFER_LENGTH 100
int log_text_len = 0;
char log_text[MAX_BUFFER_LENGTH];
FILE *outfile;
const char *filename = "test_component_sys_var_service_str.log";

#define WRITE_LOG(format, lit_log_text)                                 \
  log_text_len = sprintf(log_text, format, lit_log_text);               \
  if (fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile) != \
      static_cast<size_t>(log_text_len))                                \
    return true;

#define WRITE_LOG2(format, a1, a2)                                      \
  log_text_len = sprintf(log_text, format, a1, a2);                     \
  if (fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile) != \
      static_cast<size_t>(log_text_len))                                \
    return true;

REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_reader);

/**
  This file contains a test (example) component, which tests the services of
  "component_sys_variable_register" and component_sys_variable_unregister,
  provided by the component "mysql_server" component.
*/

static char *str_variable_value;

/**
  Initialization entry method for test component. It executes the tests of
  the service.
*/
static mysql_service_status_t test_component_sys_var_service_str_init() {
  str_variable_value = nullptr;
  unlink(filename);
  outfile = fopen(filename, "w+");

  WRITE_LOG("%s\n", "test_component_sys_var_str init:");

  STR_CHECK_ARG(str) str_arg;
  str_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component_str", "str_sys_var",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "Registering string sys_variable", nullptr, nullptr, (void *)&str_arg,
          (void *)&str_variable_value)) {
    WRITE_LOG("%s\n", "register_variable failed.");
  }

  {
    char var1[160];
    char *pvar;
    size_t len = sizeof(var1) - 1;

    pvar = &var1[0];
    if (mysql_service_mysql_system_variable_reader->get(
            nullptr, "GLOBAL", "mysql_server", "character_set_server",
            (void **)&pvar, &len)) {
      WRITE_LOG("%s\n",
                "get_variable mysql_server.character_set_server failed.");
    } else {
      WRITE_LOG2("character_set_server=[%.*s]\n", (int)len, pvar);
    }

    /* Use too small buffer, value is 8 bytes long ("utf8mb4\0"). */
    char var2[7];
    len = sizeof(var2);
    pvar = &var2[0];
    if (mysql_service_mysql_system_variable_reader->get(
            nullptr, "GLOBAL", "mysql_server", "character_set_server",
            (void **)&pvar, &len)) {
      WRITE_LOG(
          "get_variable mysql_server.character_set_server failed. "
          "The variable requires buffer %i bytes long.\n",
          (int)len);
    } else {
      WRITE_LOG2("character_set_server=[%.*s]\n", (int)len, pvar);
    }

    /* Use smallest buffer that can hold the value, value is 8 bytes long
     * ("utf8mb4\0"). */
    char var3[8];
    len = sizeof(var3);
    pvar = &var3[0];
    if (mysql_service_mysql_system_variable_reader->get(
            nullptr, "GLOBAL", "mysql_server", "character_set_server",
            (void **)&pvar, &len)) {
      WRITE_LOG(
          "get_variable mysql_server.character_set_server failed. \n"
          "The variable requires buffer %i bytes long.\n",
          (int)len);
    } else {
      WRITE_LOG2("character_set_server=[%.*s]\n", (int)len, pvar);
    }
  }
  {
    char var[FN_REFLEN];
    char *pvar;
    size_t len = sizeof(var) - 1;

    pvar = &var[0];
    if (mysql_service_mysql_system_variable_reader->get(
            nullptr, "GLOBAL", "mysql_server", "datadir", (void **)&pvar,
            &len)) {
      WRITE_LOG("%s\n", "get_variable mysql_server.datadir failed.");
    } else {
      WRITE_LOG("%s\n", "get_variable mysql_server.datadir success.");
    }
  }

  WRITE_LOG("%s\n", "test_component_sys_var_str end of init:");

  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_component_sys_var_service_str_deinit() {
  char *var_value;
  size_t len = 0;

  outfile = fopen(filename, "a+");
  WRITE_LOG("%s\n", "test_component_sys_var_str deinit:");

  var_value = new char[1024];

  len = 1024;
  if (mysql_service_mysql_system_variable_reader->get(
          nullptr, "GLOBAL", "test_component_str", "str_sys_var",
          (void **)&var_value, &len)) {
    WRITE_LOG("%s\n", "get_variable failed.");
  } else {
    WRITE_LOG("variable value : %s\n", var_value);
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "test_component_str", "str_sys_var")) {
    WRITE_LOG("%s\n", "unregister_variable failed.");
  }

  WRITE_LOG("%s\n", "test_component_sys_var_str end of deinit:");

  delete[] var_value;
  fclose(outfile);
  str_variable_value = nullptr;
  return false;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_component_sys_var_service)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_component_sys_var_service)
REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(mysql_system_variable_reader), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_sys_var_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_component_sys_var_service", "1"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_sys_var_service,
                  "mysql:test_component_sys_var_service")
test_component_sys_var_service_str_init,
    test_component_sys_var_service_str_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_sys_var_service)
    END_DECLARE_LIBRARY_COMPONENTS
