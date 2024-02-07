/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef SIMPLE_EXAMPLE_MATH_IMP_H
#define SIMPLE_EXAMPLE_MATH_IMP_H

#include <mysql/components/service_implementation.h>

/**
  A simple implementation of basic math example Service.
*/
class simple_example_math_imp {
 public:
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
  static DEFINE_BOOL_METHOD(calculate_gcd, (int a, int b, int *result));
};

#endif /* SIMPLE_EXAMPLE_MATH_IMP_H */
