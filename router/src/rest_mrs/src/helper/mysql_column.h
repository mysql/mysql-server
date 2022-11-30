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
  Column() : type_json{ColumnJsonTypes::kNull} {}
  Column(const std::string &column_name, const char *type, bool primary = false,
         bool auto_increment = false)
      : name{column_name},
        type_json{from_mysql_column_string_type(type)},
        is_primary{primary},
        is_auto_increment{auto_increment} {}
  Column(const MYSQL_FIELD *field)
      : name{field->name, field->name_length},
        type_json{from_mysql_column_type(field->type)},
        is_primary{IS_PRI_KEY(field->flags) > 0},
        is_auto_increment{(field->flags & AUTO_INCREMENT_FLAG) > 0} {}

 public:
  std::string name;
  ColumnJsonTypes type_json;
  bool is_primary{false};
  bool is_auto_increment{false};
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_COLUMN_H_
