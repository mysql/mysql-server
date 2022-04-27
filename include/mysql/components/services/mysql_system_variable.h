/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_SYSTEM_VARIABLES_H
#define MYSQL_SYSTEM_VARIABLES_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>  // MYSQL_THD
#include <mysql/components/services/mysql_string.h>  // my_h_string

/**
  @ingroup group_components_services_inventory

  Service to set the value of system variables.
  This is an example of using the service:

  @code

  MYSQL_THD thd = nullptr;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";
  const char *base_input = "my_component"; //nullptr if not a component variable
  const char *name_input = "my_variable";
  const char *value_input = "value";
  const char *type = "GLOBAL";

  my_h_string base = nullptr, name = nullptr, value = nullptr;
  if ((base != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
          &base, base_input, strlen(base_input), cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, name_input, strlen(name_input), cs) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &value, value_input, strlen(value_input), cs) {
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    if (value) mysql_service_mysql_string_factory->destroy(value);
    *error = 1;
    return 0;
  }

  if (mysql_service_mysql_system_variable_update_string->set(thd, type,
      base, name, value)) *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  if (value) mysql_service_mysql_string_factory->destroy(value);
  @endcode


  @sa @ref mysql_system_variable_update_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_system_variable_update_string)

/**
   Sets the value of a system variable to a new string value.

   @param thd thread session handle. if NULL a temp one will be created and then
   removed.
   @param set_variable_type: one of [GLOBAL, PERSIST, PERSIST_ONLY].
     If NULL, then assumes GLOBAL.
   @param variable_name_base: a mysql string of the variable name prefix. NULL
   of none
   @param variable_name: a mysql string of the variable name
   @param variable_value: a mysql string to set as value
   @retval FALSE: success
   @retval TRUE: failure, see THD if supplied.
*/
DECLARE_BOOL_METHOD(set,
                    (MYSQL_THD thd, const char *set_variable_type,
                     my_h_string variable_name_base, my_h_string variable_name,
                     my_h_string variable_value));
END_SERVICE_DEFINITION(mysql_system_variable_update_string)

/**
  @ingroup group_components_services_inventory

  Service to set the value of integer system variables.

  Passing non-NULL THD input to setter methods means that the operation will be
  executed within the scope of existing transaction, thus any operation side
  effects impacting transaction itself (for example it may generate an SQL error
  that it stores into the current THD). If using existing THD, security context
  of the thread is checked to make sure that required privileges exist. Passing
  NULL makes a temporary THD created as a execution context (and destroyed
  afterwards), i.e. no impacts on existing transactions. It doesn't make sense
  to change SESSION variable on a temporary THD, so this operation will generate
  error.

  This is an example of using the service:

  @code

  MYSQL_THD thd = nullptr;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";
  const char *base_input = "my_component"; //nullptr if not a component variable
  const char *name_input = "my_variable";
  const char *type = "SESSION";
  long long value_signed = 100000;
  unsigned long long value_unsigned = 500000;

  my_h_string base = nullptr, name = nullptr;
  if ((base != nullptr &&
      mysql_service_mysql_string_converter->convert_from_buffer(
          &base, base_input, strlen(base_input), cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, name_input, strlen(name_input), cs)) {
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    *error = 1;
    return 0;
  }

  // example call for signed integer system variable type
  if (mysql_service_mysql_system_variable_update_integer->set_signed(
         thd, type, base, name, value_signed))
    *error = 1;

  // alternative call for unsigned integer type
  if (mysql_service_mysql_system_variable_update_integer->set_unsigned(
         thd, type, base, name, value_unsigned))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  @endcode


  @sa @ref mysql_system_variable_update_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_system_variable_update_integer)

/**
   Sets the value of a system variable to a new signed integer value.

   Uses long long type to store the data, this type size may
   differ on a different environments (32-bit vs 64-bit builds for example).
   This means that the user must ensure that component using this API was
   compiled using the same environment as for the server code.

   @param thd thread session handle. if NULL a temp one will be created and then
   removed.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         If NULL, then assumes GLOBAL. SESSION is not supported
                         for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @param variable_value: long long to set as value
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
*/
DECLARE_BOOL_METHOD(set_signed,
                    (MYSQL_THD thd, const char *variable_type,
                     my_h_string variable_base, my_h_string variable_name,
                     long long variable_value));

/**
   Sets the value of a system variable to a new unsigned integer value.

   Uses unsigned long long type to store the data, this type size may
   differ on a different environments (32-bit vs 64-bit builds for example).
   This means that the user must ensure that component using this API was
   compiled using the same environment as for the server code.

   @param thd thread session handle. if NULL a temp one will be created and then
   removed.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         If NULL, then assumes GLOBAL. SESSION is not supported
                         for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @param variable_value: unsigned long long to set as value
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
 */
DECLARE_BOOL_METHOD(set_unsigned,
                    (MYSQL_THD thd, const char *variable_type,
                     my_h_string variable_base, my_h_string variable_name,
                     unsigned long long variable_value));
END_SERVICE_DEFINITION(mysql_system_variable_update_integer)

/**
  @ingroup group_components_services_inventory

  Service to set the default value of system variables.
  This is an example of using the service:

  @code

  MYSQL_THD thd = nullptr;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  const char *cs = "latin1";
  const char *base_input = "my_component"; //nullptr if not a component variable
  const char *name_input = "my_variable";
  const char *type = "PERSIST";

  my_h_string base = nullptr, name = nullptr;
  if ((base != nullptr &&
       mysql_service_mysql_string_converter->convert_from_buffer(
          &base, base_input, strlen(base_input), cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &name, name_input, strlen(name_input), cs)) {
    if (base) mysql_service_mysql_string_factory->destroy(base);
    if (name) mysql_service_mysql_string_factory->destroy(name);
    *error = 1;
    return 0;
  }

  if (mysql_service_mysql_system_variable_update_default->set(
         thd, type, base, name))
    *error = 1;

  if (base) mysql_service_mysql_string_factory->destroy(base);
  if (name) mysql_service_mysql_string_factory->destroy(name);
  @endcode


  @sa @ref mysql_system_variable_update_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_system_variable_update_default)

/**
   Sets the value of a system variable to its default value.

   @sa @ref group_components_services_inventory

   @param thd thread session handle. if NULL a temp one will be created and then
   removed.
   @param variable_type: One of GLOBAL, SESSION, PERSIST, PERSIST_ONLY.
                         If NULL, then assumes GLOBAL. SESSION is not supported
                         for a temporary THD.
   @param variable_base: a mysql string of the variable name prefix. NULL if
   none
   @param variable_name: MySQL string of the variable name
   @retval FALSE: success
   @retval TRUE: failure, error set. For error info, see THD if supplied.
*/
DECLARE_BOOL_METHOD(set,
                    (MYSQL_THD thd, const char *variable_type,
                     my_h_string variable_base, my_h_string variable_name));
END_SERVICE_DEFINITION(mysql_system_variable_update_default)

#endif /* MYSQL_SYSTEM_VARIABLES_H */
