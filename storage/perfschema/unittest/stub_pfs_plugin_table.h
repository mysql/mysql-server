/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/services/pfs_plugin_table_service.h>

#include "storage/perfschema/pfs_plugin_table.h"

static int
pfs_add_tables_v1(PFS_engine_table_share_proxy **, uint)
{
  return 0;
}

static int
pfs_delete_tables_v1(PFS_engine_table_share_proxy **, uint)
{
  return 0;
}

/* Helper functions to store/fetch value into/from a field */

/**************************************
 * Type TINYINT                       *
 **************************************/
void
set_field_tinyint_v1(PSI_field *, PSI_tinyint)
{
}
void
set_field_utinyint_v1(PSI_field *, PSI_utinyint)
{
}
void
get_field_tinyint_v1(PSI_field *, PSI_tinyint *)
{
}

/**************************************
 * Type SMALLINT                      *
 **************************************/
void
set_field_smallint_v1(PSI_field *, PSI_smallint)
{
}
void
set_field_usmallint_v1(PSI_field *, PSI_usmallint)
{
}
void
get_field_smallint_v1(PSI_field *, PSI_smallint *)
{
}

/**************************************
 * Type MEDIUMINT                     *
 **************************************/
void
set_field_mediumint_v1(PSI_field *, PSI_mediumint)
{
}
void
set_field_umediumint_v1(PSI_field *, PSI_umediumint)
{
}
void
get_field_mediumint_v1(PSI_field *, PSI_mediumint *)
{
}

/**************************************
 * Type INTEGER (INT)                 *
 **************************************/
void
set_field_integer_v1(PSI_field *, PSI_int)
{
}
void
set_field_uinteger_v1(PSI_field *, PSI_uint)
{
}
void
get_field_integer_v1(PSI_field *, PSI_int *)
{
}

/**************************************
 * Type BIGINT                        *
 **************************************/
void
set_field_bigint_v1(PSI_field *, PSI_bigint)
{
}
void
set_field_ubigint_v1(PSI_field *, PSI_ubigint)
{
}
void
get_field_bigint_v1(PSI_field *, PSI_bigint *)
{
}

/**************************************
 * Type DECIMAL                       *
 **************************************/
void
set_field_decimal_v1(PSI_field *, PSI_double)
{
}
void
get_field_decimal_v1(PSI_field *, PSI_double *)
{
}

/**************************************
 * Type FLOAT                         *
 **************************************/
void
set_field_float_v1(PSI_field *, PSI_double)
{
}
void
get_field_float_v1(PSI_field *, PSI_double *)
{
}

/**************************************
 * Type DOUBLE                        *
 **************************************/
void
set_field_double_v1(PSI_field *, PSI_double)
{
}
void
get_field_double_v1(PSI_field *, PSI_double *)
{
}

/**************************************
 * Type CHAR                          *
 **************************************/
void
set_field_char_utf8_v1(PSI_field *, const char *, unsigned int)
{
}
void
get_field_char_utf8_v1(PSI_field *, char *, unsigned int *)
{
}

/**************************************
 * Type VARCAHAR                      *
 **************************************/
void
set_field_varchar_utf8_len_v1(PSI_field *, const char *, uint)
{
}

void
set_field_varchar_utf8mb4_len_v1(PSI_field *, const char *, uint)
{
}

void
set_field_varchar_utf8_v1(PSI_field *, const char *)
{
}

void
set_field_varchar_utf8mb4_v1(PSI_field *, const char *)
{
}
void
get_field_varchar_utf8_v1(PSI_field *, char *, unsigned int *)
{
}

/**************************************
 * Type BLOB/TEXT                     *
 **************************************/
void
set_field_blob_v1(PSI_field *, const char *, uint)
{
}
void
get_field_blob_v1(PSI_field *, char *, unsigned int *)
{
}

/**************************************
 * Type ENUM                          *
 **************************************/
void
set_field_enum_v1(PSI_field *, PSI_enum)
{
}
void
get_field_enum_v1(PSI_field *, PSI_enum *)
{
}

/**************************************
 * Type DATE                          *
 **************************************/
void
set_field_date_v1(PSI_field *, const char *, uint)
{
}
void
get_field_date_v1(PSI_field *, char *, uint *)
{
}

/**************************************
 * Type TIME                          *
 **************************************/
void
set_field_time_v1(PSI_field *, const char *, uint)
{
}
void
get_field_time_v1(PSI_field *, char *, uint *)
{
}

/**************************************
 * Type DATETIME                      *
 **************************************/
void
set_field_datetime_v1(PSI_field *, const char *, uint)
{
}
void
get_field_datetime_v1(PSI_field *, char *, uint *)
{
}

/**************************************
 * Type TIMESTAMP                     *
 **************************************/
void
set_field_timestamp_v1(PSI_field *, const char *, uint)
{
}
void
get_field_timestamp_v1(PSI_field *, char *, uint *)
{
}

/**************************************
 * Type YEAR                          *
 **************************************/
void
set_field_year_v1(PSI_field *, PSI_year)
{
}
void
get_field_year_v1(PSI_field *, PSI_year *)
{
}

/**************************************
 * NULL                               *
 **************************************/
void
set_field_null_v1(PSI_field *)
{
}

void
read_key_integer_v1(PSI_key_reader *, PSI_plugin_key_integer *, int)
{
}

bool
match_key_integer_v1(bool, long, PSI_plugin_key_integer *)
{
  return false;
}

void
read_key_string_v1(PSI_key_reader *, PSI_plugin_key_string *, int)
{
}

bool
match_key_string_v1(bool, const char *, unsigned int, PSI_plugin_key_string *)
{
  return false;
}

void
init_pfs_plugin_table()
{
}

void
cleanup_pfs_plugin_table()
{
}

/* Initialization of service methods to actual PFS implementation */
SERVICE_TYPE(pfs_plugin_table)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_table){
  pfs_add_tables_v1,
  pfs_delete_tables_v1,

  set_field_tinyint_v1,
  set_field_utinyint_v1,
  get_field_tinyint_v1,
  // read_key_,
  // match_key_,

  set_field_smallint_v1,
  set_field_usmallint_v1,
  get_field_smallint_v1,
  // read_key_,
  // match_key_,

  set_field_mediumint_v1,
  set_field_umediumint_v1,
  get_field_mediumint_v1,
  // read_key_,
  // match_key_,

  set_field_integer_v1,
  set_field_uinteger_v1,
  get_field_integer_v1,
  read_key_integer_v1,
  match_key_integer_v1,

  set_field_bigint_v1,
  set_field_ubigint_v1,
  get_field_bigint_v1,
  // read_key_,
  // match_key_,

  set_field_decimal_v1,
  get_field_decimal_v1,
  // read_key_,
  // match_key_,

  set_field_float_v1,
  get_field_float_v1,
  // read_key_,
  // match_key_,

  set_field_double_v1,
  get_field_double_v1,
  // read_key_,
  // match_key_,

  set_field_char_utf8_v1,
  get_field_char_utf8_v1,
  read_key_string_v1,
  match_key_string_v1,

  set_field_varchar_utf8_v1,
  set_field_varchar_utf8_len_v1,
  get_field_varchar_utf8_v1,
  // read_key_,
  // match_key_,

  set_field_varchar_utf8mb4_v1,
  set_field_varchar_utf8mb4_len_v1,
  // read_key_,
  // match_key_,

  set_field_blob_v1,
  get_field_blob_v1,
  // read_key_,
  // match_key_,

  set_field_enum_v1,
  get_field_enum_v1,
  // read_key_,
  // match_key_,

  set_field_date_v1,
  get_field_date_v1,
  // read_key_,
  // match_key_,

  set_field_time_v1,
  get_field_time_v1,
  // read_key_,
  // match_key_,

  set_field_datetime_v1,
  get_field_datetime_v1,
  // read_key_,
  // match_key_,

  set_field_timestamp_v1,
  get_field_timestamp_v1,
  // read_key_,
  // match_key_,

  set_field_year_v1,
  get_field_year_v1,
  // read_key_,
  // match_key_,

  set_field_null_v1};
