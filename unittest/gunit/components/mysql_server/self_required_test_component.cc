/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <example_services.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <stddef.h>

#include "mysql/components/services/registry.h"

REQUIRES_SERVICE_PLACEHOLDER(example_math);

BEGIN_COMPONENT_REQUIRES(self_required_test_component)
  REQUIRES_SERVICE(example_math)
END_COMPONENT_REQUIRES()

class example_math_imp
{
public:
  static DEFINE_BOOL_METHOD(calculate_gcd,
    (int, int, int*))
  {
    return true;
  }
};

BEGIN_SERVICE_IMPLEMENTATION(self_required_test_component, example_math)
  example_math_imp::calculate_gcd,
END_SERVICE_IMPLEMENTATION()

BEGIN_COMPONENT_PROVIDES(self_required_test_component)
  PROVIDES_SERVICE(self_required_test_component, example_math)
END_COMPONENT_PROVIDES()

BEGIN_COMPONENT_METADATA(self_required_test_component)
END_COMPONENT_METADATA()

DECLARE_COMPONENT(self_required_test_component,
    "mysql:self_required_test_component")
  NULL,
  NULL
END_DECLARE_COMPONENT()

DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(self_required_test_component)
END_DECLARE_LIBRARY_COMPONENTS
