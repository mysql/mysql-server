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

#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace helper {
namespace json {
namespace sql {

using DataType = mrs::database::entry::Field::DataType;
using ColumnType = mrs::database::entry::ColumnType;

mysqlrouter::sqlstring &operator<<(mysqlrouter::sqlstring &sql,
                                   const rapidjson::Value &v) {
  const static mysqlrouter::sqlstring k_true{"TRUE"};
  const static mysqlrouter::sqlstring k_false{"FALSE"};

  if (v.IsNull()) {
    sql << nullptr;
  } else if (v.IsBool()) {
    sql << (v.GetBool() ? k_true : k_false);
  } else if (v.IsString()) {
    sql << v.GetString();
  } else if (v.IsUint()) {
    sql << v.GetUint();
  } else if (v.IsInt()) {
    sql << v.GetInt();
  } else if (v.IsUint64()) {
    sql << v.GetUint64();
  } else if (v.IsInt64()) {
    sql << v.GetInt64();
  } else if (v.IsFloat()) {
    sql << v.GetFloat();
  } else if (v.IsDouble()) {
    sql << v.GetDouble();
  } else {
    throw std::runtime_error(
        "JSON value to SQLString, received unsupported type.");
  }

  return sql;
}

static bool is_matching_type(rapidjson::Type json_type, DataType field_type) {
  switch (json_type) {
    case rapidjson::kNullType:
      return true;
    case rapidjson::kFalseType:
      return field_type == DataType::typeBoolean;
    case rapidjson::kTrueType:
      return field_type == DataType::typeBoolean;
    case rapidjson::kObjectType:
      return false;
    case rapidjson::kArrayType:
      return false;
    case rapidjson::kStringType:
      return field_type == DataType::typeString ||
             field_type == DataType::typeTimestamp;
    case rapidjson::kNumberType:
      return field_type == DataType::typeInt ||
             field_type == DataType::typeLong ||
             field_type == DataType::typeDouble;
  }

  return false;
}

static bool is_matching_type(rapidjson::Type json_type, ColumnType field_type) {
  switch (json_type) {
    case rapidjson::kNullType:
      return true;
    case rapidjson::kFalseType:
      return field_type == ColumnType::BOOLEAN;
    case rapidjson::kTrueType:
      return field_type == ColumnType::BOOLEAN;
    case rapidjson::kObjectType:
      return false;
    case rapidjson::kArrayType:
      return false;
    case rapidjson::kStringType:
      return field_type == ColumnType::STRING;
    case rapidjson::kNumberType:
      return field_type == ColumnType::INTEGER ||
             field_type == ColumnType::DOUBLE ||
             field_type == ColumnType::BOOLEAN;
  }

  return false;
}

mysqlrouter::sqlstring &operator<<(
    mysqlrouter::sqlstring &sql,
    const std::pair<rapidjson::Value *, DataType> &pair) {
  auto [v, type] = pair;

  if (is_matching_type(v->GetType(), type)) {
    sql << *v;
    return sql;
  }

  sql << json::to_string(*v);
  return sql;
}

mysqlrouter::sqlstring &operator<<(
    mysqlrouter::sqlstring &sql,
    const std::pair<rapidjson::Value *, ColumnType> &pair) {
  log_debug("operator<< (pair valie ct)");
  auto [v, type] = pair;

  if (is_matching_type(v->GetType(), type)) {
    sql << *v;
    return sql;
  }

  sql << json::to_string(*v);
  return sql;
}

}  // namespace sql
}  // namespace json
}  // namespace helper
