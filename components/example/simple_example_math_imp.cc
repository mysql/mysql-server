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

#include <mysql/components/service_implementation.h>
#include "example_services.h"
#include "simple_example_math_imp.h"

/**
  Calculates Greatest Common Divisor for given two non-negative numbers. Uses
  recursive algorithm to calculate result.

  @param a First number to calculate GCD of.
  @param b Second number to calculate GCD of.
  @param [out] result A pointer to integer variable to store result in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(simple_example_math_imp::calculate_gcd,
  (int a, int b, int* result))
{
  if (a < 0 || b < 0)
    return true;
  if (b == 0)
  {
    *result= a;
    return false;
  }
  return calculate_gcd(b, a%b, result);
}
