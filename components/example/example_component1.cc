/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

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
  @retval false success
  @retval true failure
*/
bool example_init()
{
  return false;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool example_deinit()
{
  return false;
}

/* This component provides an implementation for all example Services. */
BEGIN_SERVICE_IMPLEMENTATION(example_component1, greetings)
  english_greeting_service_imp::say_hello,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(example_component1, greetings_localization)
  english_greeting_service_imp::get_language,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(example_component1, example_math)
  simple_example_math_imp::calculate_gcd,
END_SERVICE_IMPLEMENTATION()

BEGIN_COMPONENT_PROVIDES(example_component1)
  PROVIDES_SERVICE(example_component1, greetings)
  PROVIDES_SERVICE(example_component1, greetings_localization)
  PROVIDES_SERVICE(example_component1, example_math)
END_COMPONENT_PROVIDES()

/* An empty list of dependencies. */
BEGIN_COMPONENT_REQUIRES(example_component1)
END_COMPONENT_REQUIRES()

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(example_component1)
  METADATA("mysql.author", "Oracle Corporation")
  METADATA("mysql.license", "GPL")
  METADATA("test_property", "1")
END_COMPONENT_METADATA()

/* Declaration of the Component. */
DECLARE_COMPONENT(example_component1, "mysql:example_component1")
  example_init,
  example_deinit
END_DECLARE_COMPONENT()

/* Defines list of Components contained in this library. Note that for now we
  assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(example_component1)
END_DECLARE_LIBRARY_COMPONENTS
