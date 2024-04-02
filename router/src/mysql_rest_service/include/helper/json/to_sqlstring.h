/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>

#include "mrs/database/entry/field.h"
#include "mrs/database/entry/object.h"

#include "mysqlrouter/utils_sqlstring.h"

namespace helper {
namespace json {
namespace sql {

// To not keep this function in the same namespace as to_string
mysqlrouter::sqlstring &operator<<(mysqlrouter::sqlstring &sql,
                                   const rapidjson::Value &v);

mysqlrouter::sqlstring &operator<<(
    mysqlrouter::sqlstring &sql,
    const std::pair<rapidjson::Value *, mrs::database::entry::Field::DataType>
        &v);

mysqlrouter::sqlstring &operator<<(
    mysqlrouter::sqlstring &sql,
    const std::pair<rapidjson::Value *, mrs::database::entry::ColumnType> &v);

}  // namespace sql
}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_
