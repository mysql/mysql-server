/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef FIELD_COMMON_OPERATIONS_INCLUDED
#define FIELD_COMMON_OPERATIONS_INCLUDED

#include "field_types.h"

/**
  @file field_common_properties.h

  @brief This file contains basic method for field types.

  @note This file can be imported from both the server and
  tools like mysqlbinlog, hence its simplicity
*/

/**
   Tests if field type is an integer

   @param type Field type, as returned by field->type()

   @returns true if integer type, false otherwise
 */
inline bool is_integer_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is a numeric type

  @param type Field type, as returned by field->type()

  @returns true if numeric type, false otherwise
*/
inline bool is_numeric_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is a string type

  @param type Field type, as returned by field->type()

  @returns true if string type, false otherwise
*/
inline bool is_string_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_JSON:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is temporal, i.e. represents
  DATE, TIME, DATETIME, TIMESTAMP or YEAR types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal
  @retval false  If field type is not temporal
*/
inline bool is_temporal_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_YEAR:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is temporal and has time part,
  i.e. represents TIME, DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with time part.
  @retval false  If field type is not temporal type with time part.
*/
inline bool is_temporal_type_with_time(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is temporal and has date part,
  i.e. represents DATE, DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with date part.
  @retval false  If field type is not temporal type with date part.
*/
inline bool is_temporal_type_with_date(enum_field_types type) {
  // A type which is_temporal_type() but not is_temporal_type_with_date() ?
  assert(type != MYSQL_TYPE_NEWDATE);
  switch (type) {
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;
    default:
      return false;
  }
}

/**
  Tests if field type is temporal and has date and time parts,
  i.e. represents DATETIME or TIMESTAMP types in SQL.

  @param type    Field type, as returned by field->type().
  @retval true   If field type is temporal type with date and time parts.
  @retval false  If field type is not temporal type with date and time parts.
*/
inline bool is_temporal_type_with_date_and_time(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;
    default:
      return false;
  }
}

/**
   Recognizer for concrete data type (called real_type for some reason),
   returning true if it is one of the TIMESTAMP types.
*/
inline bool is_timestamp_type(enum_field_types type) {
  return type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_TIMESTAMP2;
}

/**
  Test if the field type contains information on being signed/unsigned.
  This includes numeric but also YEAR that still contains sign modifiers
  even if ignored.

  @param type Field type, as returned by field->type()

  @returns true if the type contains info on being signed/unsigned
*/
inline bool has_signedess_information_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return true;
    default:
      return false;
  }
}

#endif /* FIELD_COMMON_OPERATIONS_INCLUDED */
