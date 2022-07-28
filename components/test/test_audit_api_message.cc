/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <ctype.h>
#include <m_string.h>
#include <my_compiler.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/audit_api_message_service.h>
#include <mysql/components/services/udf_registration.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <string>

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_audit_api_message);

/**
  Implements test_audit_api_message_internal UDF. This function generates
  AUDIT_API_MESSAGE_INTERNAL event of the AUDIT_API_MESSAGE_CLASS class.

  Although AUDIT_API_MESSAGE_INTERNAL message is generated as the result
  of the user interaction, this should not be done in the production
  environment. AUDIT_API_MESSAGE_INTERNAL message should be generated as
  as the result of the internal processing, such as background threads,
  timers etc.

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long message_internal(UDF_INIT *init [[maybe_unused]],
                                  UDF_ARGS *args [[maybe_unused]],
                                  unsigned char *null_value [[maybe_unused]],
                                  unsigned char *error [[maybe_unused]]) {
  mysql_event_message_key_value_t val;

  lex_cstring_set(&val.key, "my_numeric_key");
  val.value_type = MYSQL_AUDIT_MESSAGE_VALUE_TYPE_NUM;
  val.value.num = INT_MIN64;

  mysql_service_mysql_audit_api_message->emit(
      MYSQL_AUDIT_MESSAGE_INTERNAL, STRING_WITH_LEN("test_audit_api_message"),
      STRING_WITH_LEN("test_audit_api_message"),
      STRING_WITH_LEN("test_audit_api_message_internal"), &val, 1);

  return 0;
}

/**
  Implements test_audit_api_message_internal UDF. This function generates
  AUDIT_API_MESSAGE_USER event of the AUDIT_API_MESSAGE_CLASS class.

  @param init       Unused.
  @param args       Unused.
  @param null_value Unused.
  @param error      Unused.

  @retval 0 This function always returns 0.
*/
static long long message_user(UDF_INIT *init [[maybe_unused]],
                              UDF_ARGS *args [[maybe_unused]],
                              unsigned char *null_value [[maybe_unused]],
                              unsigned char *error [[maybe_unused]]) {
  mysql_event_message_key_value_t val;

  lex_cstring_set(&val.key, "my_string_key");
  val.value_type = MYSQL_AUDIT_MESSAGE_VALUE_TYPE_STR;
  lex_cstring_set(&val.value.str, "my_string_value");

  mysql_service_mysql_audit_api_message->emit(
      MYSQL_AUDIT_MESSAGE_USER, STRING_WITH_LEN("test_audit_api_message"),
      STRING_WITH_LEN("test_audit_api_message"),
      STRING_WITH_LEN("test_audit_api_message_user"), &val, 1);

  return 0;
}

static mysql_service_status_t init() {
  if (mysql_service_udf_registration->udf_register(
          "test_audit_api_message_internal", INT_RESULT,
          (Udf_func_any)message_internal, nullptr, nullptr))
    return true;

  if (mysql_service_udf_registration->udf_register(
          "test_audit_api_message_user", INT_RESULT, (Udf_func_any)message_user,
          nullptr, nullptr)) {
    int was_present = 0;

    mysql_service_udf_registration->udf_unregister(
        "test_audit_api_message_internal", &was_present);

    return true;
  }

  return false;
}

static mysql_service_status_t deinit() {
  int was_present = 0;

  mysql_service_udf_registration->udf_unregister(
      "test_audit_api_message_internal", &was_present);

  mysql_service_udf_registration->udf_unregister("test_audit_api_message_user",
                                                 &was_present);

  return false;
}

BEGIN_COMPONENT_PROVIDES(test_audit_api_message)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_audit_api_message)
REQUIRES_SERVICE(mysql_audit_api_message), REQUIRES_SERVICE(udf_registration),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_audit_api_message)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_audit_api_message, "test_audit_api_message")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_audit_api_message)
    END_DECLARE_LIBRARY_COMPONENTS
