/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_COLUMN_TYPES_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_COLUMN_TYPES_H_

#include <string>

#include <mysql.h>

namespace helper {

enum JsonType { kNull, kBool, kString, kNumeric, kJson, kBlob };

struct ColumnType {
  enum_field_types type_mysql{MYSQL_TYPE_NULL};
  JsonType type_json{JsonType::kNull};
  uint64_t length{0};
};

std::string txt_from_mysql_column_type(const MYSQL_FIELD *field);
JsonType from_mysql_column_string_type(const char *type);
ColumnType from_mysql_txt_column_type(const char *type);
uint64_t from_mysql_column_type_length(const char *type);
JsonType from_mysql_column_type(const MYSQL_FIELD *type);
std::string to_string(JsonType type);

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_COLUMN_TYPES_H_
