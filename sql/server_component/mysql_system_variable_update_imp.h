/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_SYSTEM_VARIABLE_IMP_H
#define MYSQL_SYSTEM_VARIABLE_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_system_variable.h>

/**
  An implementation of mysql system_variable_update_string,
  system_variable_update_integer and system_variable_update_default services for
  the mysql server component.
*/
class mysql_system_variable_update_imp {
 public:
  // system_variable_update_string service methods
  static DEFINE_BOOL_METHOD(set_string,
                            (MYSQL_THD hthd, const char *variable_type,
                             my_h_string variable_base,
                             my_h_string variable_name,
                             my_h_string variable_value));

  // system_variable_update_integer service methods
  static DEFINE_BOOL_METHOD(set_signed,
                            (MYSQL_THD hthd, const char *variable_type,
                             my_h_string variable_base,
                             my_h_string variable_name,
                             long long variable_value));
  static DEFINE_BOOL_METHOD(set_unsigned,
                            (MYSQL_THD hthd, const char *variable_type,
                             my_h_string variable_base,
                             my_h_string variable_name,
                             unsigned long long variable_value));

  // system_variable_update_default service methods
  static DEFINE_BOOL_METHOD(set_default,
                            (MYSQL_THD hthd, const char *variable_type,
                             my_h_string variable_base,
                             my_h_string variable_name));
};

#endif /* MYSQL_SYSTEM_VARIABLE_IMP_H */
