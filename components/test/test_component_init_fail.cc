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

/**
  This file contains a definition of the test_component_init_fail.
  This component is used to test a dynamic loader bahavior, which is altered
  by WL#16918.
  When component's init() fails, the component shall not get installed.
*/

#include <mysql/components/component_implementation.h>

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval allways 1 (failure)

  allways fails
*/
mysql_service_status_t test_init() { return 1; }

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval allways 0 (success)
*/
mysql_service_status_t test_deinit() { return 0; }

BEGIN_COMPONENT_PROVIDES(test_component_init_fail)
END_COMPONENT_PROVIDES();

/* A list of dependencies (no dependencies). */
BEGIN_COMPONENT_REQUIRES(test_component_init_fail)
END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_init_fail)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_init_fail, "mysql:test_component_init_fail")
test_init, test_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_init_fail)
    END_DECLARE_LIBRARY_COMPONENTS
