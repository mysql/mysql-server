/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "example_math_wrapping_imp.h"
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/registry.h>
#include "example_component3.h"
#include "example_services.h"

DEFINE_BOOL_METHOD(example_math_wrapping_imp::calculate_gcd,
                   (int a, int b, int *result)) {
  /* Try to use a dependency required by the example_component3. */
  const char *hello;
  if (mysql_service_greetings->say_hello(&hello)) {
    return true;
  }

  /* Retrieve a default Service Implementation for the example_math Service. We
    assume that Service Implementation we acquire is not the one that this
    implementation is. */
  my_service<SERVICE_TYPE(example_math)> service("example_math",
                                                 mysql_service_registry);
  if (service) {
    return true;
  }
  return service->calculate_gcd(a, b, result);
}
