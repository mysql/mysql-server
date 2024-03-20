/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INCLUDED
#define MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/global_variable_attributes_bits.h>

/*
  Version 1.
  Introduced in MySQL 9.0.0
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_global_variable_attributes)

global_variable_attributes_assign_t set;
global_variable_attributes_get_t get;
global_variable_attributes_get_time_t get_time;
global_variable_attributes_get_user_t get_user;

END_SERVICE_DEFINITION(mysql_global_variable_attributes)

/*
  Version 1.
  Introduced in MySQL 9.0.0
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_global_variable_attributes_iterator)

global_variable_attributes_iterator_create_t create;
global_variable_attributes_iterator_destroy_t destroy;
global_variable_attributes_iterator_advance_t advance;
global_variable_attributes_iterator_get_name_t get_name;
global_variable_attributes_iterator_get_value_t get_value;

END_SERVICE_DEFINITION(mysql_global_variable_attributes_iterator)

#endif /* MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INCLUDED */
