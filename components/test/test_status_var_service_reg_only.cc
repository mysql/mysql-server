/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include <fcntl.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typelib.h"

#define MAX_BUFFER_LENGTH 100
int log_text_len = 0;
char log_text[MAX_BUFFER_LENGTH];
FILE *outfile;
const char *filename = "test_component_status_var_service_reg_only.log";

#define WRITE_LOG(format, lit_log_text)                                 \
  log_text_len = sprintf(log_text, format, lit_log_text);               \
  if (fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile) != \
      static_cast<size_t>(log_text_len))                                \
    return true;

REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);

static char str_value[32];
static int int_variable_value = 4096;
static int int_var_2_value = 2048;
static unsigned int uint_variable_value = 0;

SHOW_VAR int_variable[] = {
    {"test_int_component.int_variable", (char *)&int_variable_value, SHOW_INT,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};
SHOW_VAR int_var_2[] = {
    {"int_var_2", (char *)&int_var_2_value, SHOW_INT, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};
SHOW_VAR uint_variable[] = {
    {"test_int_component.uint_variable", (char *)&uint_variable_value, SHOW_INT,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};
SHOW_VAR str_variable[] = {
    {"test_str_component.str_variable", (char *)&str_value, SHOW_CHAR,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

/*
  Initialization entry method for test component. It executes the tests of
  the service.
*/
static mysql_service_status_t test_component_status_var_service_reg_init() {
  outfile = fopen(filename, "w+");

  WRITE_LOG("%s\n", "test_component_status_var_reg init:");

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&int_variable)) {
    WRITE_LOG("%s\n", "int register_variable failed.");
  }

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&int_var_2)) {
    WRITE_LOG("%s\n", "int register_variable_2 failed.");
  }

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&uint_variable)) {
    WRITE_LOG("%s\n", "uint register_variable failed.");
  }

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&str_variable)) {
    WRITE_LOG("%s\n", "str register_variable failed.");
  }

  strcpy(str_value, "This is the initial text value.");

  WRITE_LOG("%s\n", "test_component_status_var_reg end of init:");
  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_component_status_var_service_reg_deinit() {
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
test_component_status_var_service_reg_init,
    test_component_status_var_service_reg_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_status_var_service)
    END_DECLARE_LIBRARY_COMPONENTS
