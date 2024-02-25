/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_STATUS_VARIABLE_READER_H
#define MYSQL_STATUS_VARIABLE_READER_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/thd.h>
#include <mysql/components/services/mysql_string.h>

/**
  @file
  Read status variable values service(s)
*/

/**
  @ingroup group_components_services_inventory

  Service to read the value of a status variable as a string.

  Used approximately as follows:

  @code
  my_h_string value = nullptr;
  bool get_global = true; // or false
  MYSQL_THD thd = nullptr; // use a temp thd
  const char *name = "gizmo";
  char buffer[1024];
  const char *charset= "utf8mb4";

  // try to get the current thd if there is one
  if (!get_global && mysql_service_mysql_current_thread_reader->get(&thd)) {
    // failed to get the current THD
    *is_null = 1;
    *error = 1;
    return nullptr;
  }
  if (!mysql_service_mysql_status_variable_string->get(thd, name, get_global,
  &str) && str && !mysql_service_mysql_string_converter->convert_to_buffer( str,
  buffer, sizeof(buffer) - 1, charset)) {
    mysql_service_mysql_string_factory->destroy(str);
    // the value is in buffer
  }

  // wasn't able to get the value. do cleanup
  if (str) mysql_service_mysql_string_factory->destroy(str);
  @endcode

  @sa mysql_status_variable_reader_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_status_variable_string)

/**
  Gets the string value of a status variable

  Will read the value of the status variable supplied, convert to a string
  as needed and store into the out_string parameter as a @ref my_h_string.

  @note  No status variable values aggregation will be done from the threads
  to the globals. Will keep the status variable lock while extracting the value.

  @warning The function will return an error if you do not supply a session
    and request the session value.

  @param       thd The THD to operate on or a nullptr to create a temp THD
  @param       name The name of the status variable to access. US ASCII
  @param       get_global true if the global value of the status variable is
  required, false if the session one.
  @param [out] out_string a place to store the value
  @return Status of the operation
  @retval false success
  @retval true failure

  @sa mysql_status_variable_reader_imp::get
*/
DECLARE_BOOL_METHOD(get, (MYSQL_THD thd, const char *name, bool get_global,
                          my_h_string *out_string));

END_SERVICE_DEFINITION(mysql_status_variable_string)

#endif /* MYSQL_STATUS_VARIABLE_READER_H */
