/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>

/**
  This file contains a definition of the test_component_init_then_register.
  This component is used to test a dynamic loader bahavior, which is altered
  by WL#16918. Dynamic loader load operation is expected to invoke the
  component init() first and only then register component's provided services
  into the registry.
*/

REQUIRES_SERVICE_PLACEHOLDER(registry_registration);

namespace do_nothing_service_imp {
static DEFINE_BOOL_METHOD(do_nothing, ());
}  // namespace do_nothing_service_imp

DEFINE_BOOL_METHOD(do_nothing_service_imp::do_nothing, ()) { return false; }

BEGIN_SERVICE_DEFINITION(do_nothing_service)
DECLARE_BOOL_METHOD(do_nothing, ());
END_SERVICE_DEFINITION(do_nothing_service)

BEGIN_SERVICE_IMPLEMENTATION(test_component_init_then_register,
                             do_nothing_service)
do_nothing_service_imp::do_nothing, END_SERVICE_IMPLEMENTATION();

static bool component_is_initialized = false;

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t test_init() {
  if (component_is_initialized) return 1;

  // we register the service which is also a PROVIDED service.
  // If the service is registered already, then again registration will fail.
  // otherwise it will succed.
  if (mysql_service_registry_registration->register_service(
          "do_nothing_service.test_component_init_then_register",
          reinterpret_cast<
              my_h_service>(const_cast<mysql_service_do_nothing_service_t *>(
              &imp_test_component_init_then_register_do_nothing_service)))) {
    return 1;
  }

  // we do cleanup, so dynamic loader can register the provided service.
  if (mysql_service_registry_registration->unregister(
          "do_nothing_service.test_component_init_then_register")) {
    return 1;
  }

  component_is_initialized = true;
  return 0;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t test_deinit() {
  if (!component_is_initialized) return 1;
  component_is_initialized = false;
  return 0;
}

BEGIN_COMPONENT_PROVIDES(test_component_init_then_register)
PROVIDES_SERVICE(test_component_init_then_register, do_nothing_service),
    END_COMPONENT_PROVIDES();

/* A list of dependencies. */
BEGIN_COMPONENT_REQUIRES(test_component_init_then_register)
REQUIRES_SERVICE(registry_registration), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_init_then_register)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_init_then_register,
                  "mysql:test_component_init_then_register")
test_init, test_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_init_then_register)
    END_DECLARE_LIBRARY_COMPONENTS
