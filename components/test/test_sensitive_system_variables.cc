/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/plugin.h>

#include "my_macros.h"
#include "scope_guard.h"
#include "typelib.h"

REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);

/**
  This file contains a test (example) component,
  which tests the SENSITIVE variables functionality.
*/

static char *sensitive_string_1 = nullptr;
static char *sensitive_string_2 = nullptr;
static char *sensitive_string_3 = nullptr;

static char *sensitive_ro_string_1 = nullptr;
static char *sensitive_ro_string_2 = nullptr;
static char *sensitive_ro_string_3 = nullptr;

/**
  De-initialization method for Component
*/
static mysql_service_status_t
test_component_sensitive_system_variables_service_deinit() {
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_string_1");
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_string_2");
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_string_3");
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_ro_string_1");
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_ro_string_2");
  (void)mysql_service_component_sys_variable_unregister->unregister_variable(
      "test_component", "sensitive_ro_string_3");

  sensitive_string_1 = nullptr;
  sensitive_string_2 = nullptr;
  sensitive_string_3 = nullptr;
  sensitive_ro_string_1 = nullptr;
  sensitive_ro_string_2 = nullptr;
  sensitive_ro_string_3 = nullptr;

  return false;
}

/**
  Initialization method for Component
*/
static mysql_service_status_t
test_component_sensitive_system_variables_service_init() {
  STR_CHECK_ARG(str)
  str_1_arg, str_2_arg, str_3_arg, ro_str_1_arg, ro_str_2_arg, ro_str_3_arg;
  str_1_arg.def_val = nullptr;

  auto cleanup = create_scope_guard(
      [] { test_component_sensitive_system_variables_service_deinit(); });
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_string_1",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_SENSITIVE,
          "Sensitive string variable 1", nullptr, nullptr, (void *)&str_1_arg,
          (void *)&sensitive_string_1)) {
    return 1;
  }

  str_2_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_string_2",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_SENSITIVE,
          "Sensitive string variable 2", nullptr, nullptr, (void *)&str_2_arg,
          (void *)&sensitive_string_2)) {
    return 1;
  }

  str_3_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_string_3",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_SENSITIVE,
          "Sensitive string variable 3", nullptr, nullptr, (void *)&str_3_arg,
          (void *)&sensitive_string_3)) {
    return 1;
  }

  ro_str_1_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_ro_string_1",
          PLUGIN_VAR_STR | PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
              PLUGIN_VAR_SENSITIVE,
          "Sensitive Read-Only string variable 1", nullptr, nullptr,
          (void *)&ro_str_1_arg, (void *)&sensitive_ro_string_1)) {
    return 1;
  }

  ro_str_2_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_ro_string_2",
          PLUGIN_VAR_STR | PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
              PLUGIN_VAR_SENSITIVE,
          "Sensitive Read-Only string variable 2", nullptr, nullptr,
          (void *)&ro_str_2_arg, (void *)&sensitive_ro_string_2)) {
    return 1;
  }

  ro_str_3_arg.def_val = nullptr;
  if (mysql_service_component_sys_variable_register->register_variable(
          "test_component", "sensitive_ro_string_3",
          PLUGIN_VAR_STR | PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
              PLUGIN_VAR_SENSITIVE,
          "Sensitive Read-Only string variable 3", nullptr, nullptr,
          (void *)&ro_str_3_arg, (void *)&sensitive_ro_string_3)) {
    return 1;
  }

  cleanup.commit();
  return 0;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_component_sensitive_system_variables_service)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_component_sensitive_system_variables_service)
REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_sensitive_system_variables_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_component_sensitive_system_variables_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_sensitive_system_variables_service,
                  "mysql:test_component_sensitive_system_variables_service")
test_component_sensitive_system_variables_service_init,
    test_component_sensitive_system_variables_service_deinit
    END_DECLARE_COMPONENT();

/*
  Defines list of Components contained in this library.
  Note that for now we assume that library will have
  exactly one Component.
*/
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(
    test_component_sensitive_system_variables_service)
    END_DECLARE_LIBRARY_COMPONENTS
