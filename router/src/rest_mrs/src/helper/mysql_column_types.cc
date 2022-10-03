/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "helper/mysql_column_types.h"

namespace helper {

ColumnJsonTypes from_mysql_column_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
      return helper::ColumnJsonTypes::kNumeric;

    case MYSQL_TYPE_TYPED_ARRAY:
    case MYSQL_TYPE_INVALID:
    case MYSQL_TYPE_NULL:
      return helper::ColumnJsonTypes::kNull;

    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
      return helper::ColumnJsonTypes::kString;

    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_BOOL:
      return helper::ColumnJsonTypes::kBool;

    case MYSQL_TYPE_JSON:
      return helper::ColumnJsonTypes::kJson;

    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_STRING:
      return helper::ColumnJsonTypes::kString;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      return helper::ColumnJsonTypes::kString;
  }

  return helper::ColumnJsonTypes::kNull;
}

std::string to_string(ColumnJsonTypes type) {
  switch (type) {
    case kNull:
      return "null";
    case kBool:
      return "boolean";
    case kString:
      return "string";
    case kNumeric:
      return "numeric";
    case kJson:
      return "json";
  }
  return "null";
}

}  // namespace helper
