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

#include <mysql/components/services/registry.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/my_service.h>
#include "example_component3.h"
#include "example_math_wrapping_imp.h"
#include "example_services.h"

DEFINE_BOOL_METHOD(example_math_wrapping_imp::calculate_gcd,
  (int a, int b, int* result))
{
  /* Try to use a dependency required by the example_component3. */
  const char* hello;
  if (mysql_service_greetings->say_hello(&hello))
  {
    return true;
  }

  /* Retrieve a default Service Implementation for the example_math Service. We
    assume that Service Implementation we acquire is not the one that this
    implementation is. */
  my_service<SERVICE_TYPE(example_math)> service("example_math",
    mysql_service_registry);
  if (service)
  {
    return true;
  }
  return service->calculate_gcd(a, b, result);
}
