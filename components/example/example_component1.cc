/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
#include "english_greeting_service_imp.h"
#include "example_services.h"
#include "simple_example_math_imp.h"

/**
  This file contains a definition of the example_component1.
*/

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t example_init() { return 0; }

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t example_deinit() { return 0; }

/* This component provides an implementation for all example Services. */
BEGIN_SERVICE_IMPLEMENTATION(example_component1, greetings)
english_greeting_service_imp::say_hello, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(example_component1, greetings_localization)
english_greeting_service_imp::get_language, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(example_component1, example_math)
simple_example_math_imp::calculate_gcd, END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(example_component1)
PROVIDES_SERVICE(example_component1, greetings),
    PROVIDES_SERVICE(example_component1, greetings_localization),
    PROVIDES_SERVICE(example_component1, example_math),
    END_COMPONENT_PROVIDES();

/* An empty list of dependencies. */
BEGIN_COMPONENT_REQUIRES(example_component1) END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(example_component1)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(example_component1, "mysql:example_component1")
example_init, example_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(example_component1)
    END_DECLARE_LIBRARY_COMPONENTS
