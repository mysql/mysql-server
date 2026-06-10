/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _MYSQL_REST_SERVICE_INCLUDE_HELPER_SQLSTRING_UTILS_H_
#define _MYSQL_REST_SERVICE_INCLUDE_HELPER_SQLSTRING_UTILS_H_

#include <string>
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace helper {

using DataType = mrs::database::entry::ColumnType;

inline mysqlrouter::sqlstring get_sql_format(DataType type) {
  using namespace helper;
  switch (type) {
    case DataType::BINARY:
      return mysqlrouter::sqlstring("FROM_BASE64(?)");

    case DataType::GEOMETRY:
      return mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?)");

    case DataType::VECTOR:
      return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)");

    case DataType::JSON:
      return mysqlrouter::sqlstring("CAST(? as JSON)");

    default: {
    }
  }

  return mysqlrouter::sqlstring("?");
}

inline mysqlrouter::sqlstring get_sql_formatted(const rapidjson::Value &value,
                                                DataType type) {
  using namespace helper::json::sql;

  auto tmp = get_sql_format(type);
  tmp << value;
  return tmp;
}

}  // namespace helper

#endif /* _MYSQL_REST_SERVICE_INCLUDE_HELPER_SQLSTRING_UTILS_H_ */
