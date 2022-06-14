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

  Service to set the global value of system variables.
  This is an example of using the service in an UDF:

  @code

  MYSQL_THD thd = NULL;

  if (!make_new_thread &&
      mysql_service_mysql_current_thread_reader->get(&thd)) {
    *error = 1;
    return 0;
  }

  void *arg1_cs, *arg2_cs;
  if (mysql_service_mysql_udf_metadata->argument_get(args, "charset", 1,
                                                     &arg1_cs) ||
      mysql_service_mysql_udf_metadata->argument_get(args, "charset", 2,
                                                     &arg2_cs)) {
    *error = 1;
    return 0;
  }

  my_h_string name = nullptr, value = nullptr;
  if (mysql_service_mysql_string_converter->convert_from_buffer(
          &name, args->args[1], args->lengths[1], ((const char *)arg1_cs)) ||
      mysql_service_mysql_string_converter->convert_from_buffer(
          &value, args->args[2], args->lengths[2], ((const char *)arg2_cs))) {
    if (name) mysql_service_mysql_string_factory->destroy(name);
    if (value) mysql_service_mysql_string_factory->destroy(value);
    *error = 1;
    return 0;
  }

  if (mysql_service_mysql_system_variable_update_string->set(thd, args->args[3],
      nullptr, name, value)) *error = 1;

  if (name) mysql_service_mysql_string_factory->destroy(name);
  if (value) mysql_service_mysql_string_factory->destroy(value);
  @endcode


  @sa @ref mysql_system_variable_update_string_imp
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

#endif /* MYSQL_SYSTEM_VARIABLES_H */
