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

#ifndef EXAMPLE_SERVICES_H
#define EXAMPLE_SERVICES_H

#include <mysql/components/service.h>

/**
  A Service to get greeting message.
*/
BEGIN_SERVICE_DEFINITION(greetings)
/**
  Retrieves a greeting message.

  @param [out] hello_string A pointer to string data pointer to store result
    in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(say_hello, (const char **hello_string));
END_SERVICE_DEFINITION(greetings)

/**
  A Service to get localization information on related greetings Service.
*/
BEGIN_SERVICE_DEFINITION(greetings_localization)
/**
  Retrieves a greeting message language of related greeting Service.

  @param [out] language_string A pointer to string data pointer to store name
    of the language in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get_language, (const char **language_string));
END_SERVICE_DEFINITION(greetings_localization)

/**
  A Service for example basic math functionality.
*/
BEGIN_SERVICE_DEFINITION(example_math)
/**
  Calculates Greatest Common Divisor for given two non-negative numbers.

  @param a First number to calculate GCD of.
  @param b Second number to calculate GCD of.
  @param [out] result A pointer to integer variable to store result in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(calculate_gcd, (int a, int b, int *result));
END_SERVICE_DEFINITION(example_math)

#endif /* EXAMPLE_SERVICES_H */
