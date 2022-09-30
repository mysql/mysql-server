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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_COLUMN_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_COLUMN_H_

#include <string>

#include <mysql.h>

#include "helper/mysql_column_types.h"

namespace helper {

struct Column {
  Column() : type_json{ColumnJsonTypes::kNull}, type_mysql{MYSQL_TYPE_NULL} {}
  Column(const std::string &column_name, enum_field_types type,
         bool primary = false)
      : name{column_name},
        type_json{from_mysql_column_type(type)},
        type_mysql{type},
        is_primary{primary} {}
  Column(const MYSQL_FIELD *field)
      : name{field->name, field->name_length},
        type_json{from_mysql_column_type(field->type)},
        type_mysql{field->type},
        is_primary{IS_PRI_KEY(field->flags) > 0} {}

 public:
  std::string name;
  ColumnJsonTypes type_json;
  enum_field_types type_mysql;
  bool is_primary{false};
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_COLUMN_H_
