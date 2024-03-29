/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_QUERY_ATTRIBUTES_IMP_H
#define MYSQL_QUERY_ATTRIBUTES_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_query_attributes.h>

/**
  Query attributes service implementation.
*/
class mysql_query_attributes_imp {
 public:
  // iterator methods

  static DEFINE_BOOL_METHOD(create,
                            (MYSQL_THD thd, const char *name,
                             mysqlh_query_attributes_iterator *out_iterator));
  static DEFINE_BOOL_METHOD(get_type, (mysqlh_query_attributes_iterator iter,
                                       enum enum_field_types *out_type));
  static DEFINE_BOOL_METHOD(next, (mysqlh_query_attributes_iterator iter));
  static DEFINE_BOOL_METHOD(get_name, (mysqlh_query_attributes_iterator iter,
                                       my_h_string *out_name_handle));
  static DEFINE_METHOD(void, release, (mysqlh_query_attributes_iterator iter));

  // string methods
  static DEFINE_BOOL_METHOD(string_get, (mysqlh_query_attributes_iterator iter,
                                         my_h_string *out_string_value));

  // is null methods
  static DEFINE_BOOL_METHOD(isnull_get, (mysqlh_query_attributes_iterator iter,
                                         bool *out_null));
};

#endif /* MYSQL_QUERY_ATTRIBUTES_IMP_H */
