/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>
#include "udf_extension_test_functions.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);

/**
  Plugin init function that registers a UDF.
  A newly created UDF must be registered here.

  @retval false UDF registered successfully.
  @retval true  Otherwise.
*/
static int test_udf_extension_init() {
  bool ret = true;
  /*
    Demonstrates how to set and get the charset extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_result_charset", STRING_RESULT,
          (Udf_func_any)test_result_charset, test_result_charset_init,
          test_result_charset_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected charset of a UDF argument.
    Users sets the charset of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the charset of first UDF
    argument as charset of second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_args_charset", STRING_RESULT, (Udf_func_any)test_args_charset,
          test_args_charset_init, test_args_charset_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the collation extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument. It determines
    the charset of first argument from the collation name as it was set
    during init() time.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_result_collation", STRING_RESULT,
          (Udf_func_any)test_result_collation, test_result_collation_init,
          test_result_collation_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected collation of a UDF argument.
    Users sets the collation of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the collation of first UDF
    argument as collation of second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_args_collation", STRING_RESULT,
          (Udf_func_any)test_args_collation, test_args_collation_init,
          test_args_collation_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the charset extension argument of
    return value. It also demonstrate how to perforn the charset conversion
    on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set as it was specified by the user in the second
    argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_result_charset_with_value", STRING_RESULT,
          (Udf_func_any)test_result_charset_with_value,
          test_result_charset_with_value_init,
          test_result_charset_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected charset of a UDF argument.
    Users sets the charset of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the charset of first UDF
    argument as charset provided by the user in the second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_args_charset_with_value", STRING_RESULT,
          (Udf_func_any)test_args_charset_with_value,
          test_args_charset_with_value_init,
          test_args_charset_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the collation extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument. It determines
    the charset of first argument from the collation name as provided
    by the user in the second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_result_collation_with_value", STRING_RESULT,
          (Udf_func_any)test_result_collation_with_value,
          test_result_collation_with_value_init,
          test_result_collation_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected collation of a UDF argument.
    Users sets the collation of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the collation of first UDF
    argument as collation provided by the user in the second argument.
  */
  if (mysql_service_udf_registration->udf_register(
          "test_args_collation_with_value", STRING_RESULT,
          (Udf_func_any)test_args_collation_with_value,
          test_args_collation_with_value_init,
          test_args_collation_with_value_deinit)) {
    goto end;
  }

  if (mysql_service_udf_registration->udf_register(
          "test_args_without_init_deinit_methods", STRING_RESULT,
          (Udf_func_any)test_args_without_init_deinit_methods, nullptr,
          nullptr)) {
    goto end;
  }
  ret = false;  // Successfully initialized the plugin
end:
  return ret ? 1 : 0;
}

/**
  Plugin deinit function that unregisters a UDF

  @retval false UDF unregistered successfully.
  @retval true  Otherwise.
*/
static int test_udf_extension_deinit() {
  bool ret = true;
  int was_present;
  if (mysql_service_udf_registration->udf_unregister("test_result_charset",
                                                     &was_present) ||
      mysql_service_udf_registration->udf_unregister("test_args_charset",
                                                     &was_present) ||
      mysql_service_udf_registration->udf_unregister("test_result_collation",
                                                     &was_present) ||
      mysql_service_udf_registration->udf_unregister("test_args_collation",
                                                     &was_present) ||
      mysql_service_udf_registration->udf_unregister(
          "test_result_charset_with_value", &was_present) ||
      mysql_service_udf_registration->udf_unregister(
          "test_args_charset_with_value", &was_present) ||
      mysql_service_udf_registration->udf_unregister(
          "test_result_collation_with_value", &was_present) ||
      mysql_service_udf_registration->udf_unregister(
          "test_args_collation_with_value", &was_present) ||
      mysql_service_udf_registration->udf_unregister(
          "test_args_without_init_deinit_methods", &was_present)) {
    goto end;
  }
  ret = false;
end:
  return ret ? 1 : 0;
}

BEGIN_COMPONENT_PROVIDES(test_udf_extension)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_udf_extension)
REQUIRES_SERVICE(udf_registration), REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_string_factory),
    REQUIRES_SERVICE(mysql_udf_metadata), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_udf_extension)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_udf_extension, "mysql:test_udf_extension")
test_udf_extension_init, test_udf_extension_deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_udf_extension)
    END_DECLARE_LIBRARY_COMPONENTS
