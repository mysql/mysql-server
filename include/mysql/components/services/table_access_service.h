/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_TABLE_ACCESS_SERVICE_H
#define COMPONENTS_SERVICES_TABLE_ACCESS_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/table_access_bits.h>

/**
  @defgroup group_table_access_services Table Access services
  @ingroup group_components_services_inventory
  @{
*/

/**
  Table access service, factory.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(table_access_factory_v1)
/** @sa create_table_access_v1_t */
create_table_access_v1_t create;
/** @sa destroy_table_access_v1_t */
destroy_table_access_v1_t destroy;
END_SERVICE_DEFINITION(table_access_factory_v1)

/**
  Table access service, table access.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(table_access_v1)
/** @sa add_table_v1_t */
add_table_v1_t add;
/** @sa begin_v1_t */
begin_v1_t begin;
/** @sa commit_v1_t */
commit_v1_t commit;
/** @sa rollback_v1_t */
rollback_v1_t rollback;
/** @sa get_table_v1_t */
get_table_v1_t get;
/** @sa check_table_fields_v1_t */
check_table_fields_v1_t check;
END_SERVICE_DEFINITION(table_access_v1)

/**
  Table access service, index scan.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(table_access_index_v1)
/** @sa index_init_v1_t */
index_init_v1_t init;
/** @sa index_read_map_v1_t */
index_read_map_v1_t read_map;
/** @sa index_first_v1_t */
index_first_v1_t first;
/** @sa index_next_v1_t */
index_next_v1_t next;
/** @sa index_next_same_v1_t */
index_next_same_v1_t next_same;
/** @sa index_end_v1_t */
index_end_v1_t end;
END_SERVICE_DEFINITION(table_access_index_v1)

/**
  Table access service, table scan.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(table_access_scan_v1)
/** @sa rnd_init_v1_t */
rnd_init_v1_t init;
/** @sa rnd_next_v1_t */
rnd_next_v1_t next;
/** @sa rnd_end_v1_t */
rnd_end_v1_t end;
END_SERVICE_DEFINITION(table_access_scan_v1)

/**
  Table access service, update.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(table_access_update_v1)
/** @sa write_row_v1_t */
write_row_v1_t insert;
/** @sa update_row_v1_t */
update_row_v1_t update;
/** @sa delete_row_v1_t */
delete_row_v1_t delete_row;
END_SERVICE_DEFINITION(table_access_update_v1)

/**
  Table access service, all columns.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(field_access_nullability_v1)
/** @sa set_field_null_v1_t */
set_field_null_v1_t set;
/** @sa is_field_null_v1_t */
is_field_null_v1_t get;
END_SERVICE_DEFINITION(field_access_nullability_v1)

/**
  Table access service, integer columns.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(field_integer_access_v1)
/** @sa set_field_integer_value_v1_t */
set_field_integer_value_v1_t set;
/** @sa get_field_integer_value_v1_t */
get_field_integer_value_v1_t get;
END_SERVICE_DEFINITION(field_integer_access_v1)

/**
  Table access service, varchar columns.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(field_varchar_access_v1)
/** @sa set_field_varchar_value_v1_t */
set_field_varchar_value_v1_t set;
/** @sa get_field_varchar_value_v1_t */
get_field_varchar_value_v1_t get;
END_SERVICE_DEFINITION(field_varchar_access_v1)

/**
  Table access service, any columns.
  Version 1.
  Status: active
*/
BEGIN_SERVICE_DEFINITION(field_any_access_v1)
/** @sa set_field_any_value_v1_t */
set_field_any_value_v1_t set;
/** @sa get_field_any_value_v1_t */
get_field_any_value_v1_t get;
END_SERVICE_DEFINITION(field_any_access_v1)

/**
  @} (end of group_table_access_services)
*/

#endif /* COMPONENTS_SERVICES_TABLE_ACCESS_SERVICE_H */
