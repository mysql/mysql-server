/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "polish_greeting_service_imp.h"
#include "example_services.h"

/**
  This file contains a definition of the example_component2.
*/

/* This component provides an implementation for greeting-related example
  Services. */

BEGIN_SERVICE_IMPLEMENTATION(example_component2, greetings)
  polish_greeting_service_imp::say_hello,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(example_component2, greetings_localization)
  polish_greeting_service_imp::get_language,
END_SERVICE_IMPLEMENTATION()

BEGIN_COMPONENT_PROVIDES(example_component2)
  PROVIDES_SERVICE(example_component2, greetings)
  PROVIDES_SERVICE(example_component2, greetings_localization)
END_COMPONENT_PROVIDES()

/* An empty list of dependencies. */
BEGIN_COMPONENT_REQUIRES(example_component2)
END_COMPONENT_REQUIRES()

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(example_component2)
  METADATA("mysql.author", "Oracle Corporation")
  METADATA("mysql.license", "GPL")
  METADATA("test_property", "2")
END_COMPONENT_METADATA()

/* Declaration of the Component, this is the case when we don't need
  initialization/de-initialization methods, so we are fine to just use NULL
  values here. */
DECLARE_COMPONENT(example_component2, "mysql:example_component2")
  NULL,
  NULL
END_DECLARE_COMPONENT()

/* Defines list of Components contained in this library. Note that for now we
  assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(example_component2)
END_DECLARE_LIBRARY_COMPONENTS
