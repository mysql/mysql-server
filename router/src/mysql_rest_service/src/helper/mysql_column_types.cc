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

#include "helper/mysql_column_types.h"

#include <cstring>
#include <map>
#include <string_view>

#include "my_sys.h"
#include "mysql/harness/string_utils.h"
#include "mysql/strings/m_ctype.h"

namespace helper {

static const auto &get_txt_type_mapping() {
  const static std::map<std::string, ColumnType> map{
      {"boolean", {MYSQL_TYPE_BOOL, helper::JsonType::kBool}},
      {"bit", {MYSQL_TYPE_BIT, helper::JsonType::kBlob}},

      {"json", {MYSQL_TYPE_JSON, helper::JsonType::kJson}},

      {"tinyint", {MYSQL_TYPE_TINY, helper::JsonType::kNumeric}},
      {"smallint", {MYSQL_TYPE_SHORT, helper::JsonType::kNumeric}},
      {"int", {MYSQL_TYPE_LONG, helper::JsonType::kNumeric}},
      {"float", {MYSQL_TYPE_FLOAT, helper::JsonType::kNumeric}},
      {"double", {MYSQL_TYPE_DOUBLE, helper::JsonType::kNumeric}},
      {"bigint", {MYSQL_TYPE_LONGLONG, helper::JsonType::kNumeric}},
      {"mediumint", {MYSQL_TYPE_INT24, helper::JsonType::kNumeric}},
      {"decimal", {MYSQL_TYPE_DECIMAL, helper::JsonType::kNumeric}},

      {"null", {MYSQL_TYPE_NULL, helper::JsonType::kNull}},

      {"char", {MYSQL_TYPE_VARCHAR, helper::JsonType::kString}},
      {"set", {MYSQL_TYPE_SET, helper::JsonType::kString}},
      {"enum", {MYSQL_TYPE_ENUM, helper::JsonType::kString}},
      {"text", {MYSQL_TYPE_STRING, helper::JsonType::kString}},
      {"longtext", {MYSQL_TYPE_STRING, helper::JsonType::kString}},
      {"mediumtext", {MYSQL_TYPE_STRING, helper::JsonType::kString}},
      {"tinytext", {MYSQL_TYPE_STRING, helper::JsonType::kString}},
      {"varchar", {MYSQL_TYPE_VARCHAR, helper::JsonType::kString}},

      {"geometry", {MYSQL_TYPE_GEOMETRY, helper::JsonType::kString}},

      {"timestamp", {MYSQL_TYPE_TIMESTAMP, helper::JsonType::kString}},
      {"date", {MYSQL_TYPE_DATE, helper::JsonType::kString}},
      {"time", {MYSQL_TYPE_TIME, helper::JsonType::kString}},
      {"datetime", {MYSQL_TYPE_DATETIME, helper::JsonType::kString}},
      {"year", {MYSQL_TYPE_YEAR, helper::JsonType::kString}},

      {"binary", {MYSQL_TYPE_BLOB, helper::JsonType::kBlob}},
      {"tinyblob", {MYSQL_TYPE_TINY_BLOB, helper::JsonType::kBlob}},
      {"mediumblob", {MYSQL_TYPE_MEDIUM_BLOB, helper::JsonType::kBlob}},
      {"longblob", {MYSQL_TYPE_LONG_BLOB, helper::JsonType::kBlob}},
      {"blob", {MYSQL_TYPE_BLOB, helper::JsonType::kBlob}},
  };

  return map;
}

static std::string appedn_unsigned(const MYSQL_FIELD *field) {
  return (field->flags & UNSIGNED_FLAG) ? " UNSIGNED" : "";
}

static std::string append_length(const MYSQL_FIELD *field) {
  using namespace std::string_literals;
  auto l = field->length;
  auto cs = get_charset(field->charsetnr, 0);
  if (cs) {
    l = l / cs->mbmaxlen;
  }

  return "("s + std::to_string(l) + ")";
}

static std::string append_length_dec(const MYSQL_FIELD *field) {
  using namespace std::string_literals;
  return "("s + std::to_string(field->max_length) + "," +
         std::to_string(field->decimals) + ")";
}

std::string txt_from_mysql_column_type(const MYSQL_FIELD *field) {
  using namespace std::string_literals;
  switch (field->type) {
    case MYSQL_TYPE_DECIMAL:
      return "DECIMAL"s + append_length_dec(field);
    case MYSQL_TYPE_NEWDECIMAL:
      return "DECIMAL"s + append_length_dec(field);
    case MYSQL_TYPE_TINY:
      return "TINYINT"s + appedn_unsigned(field);
    case MYSQL_TYPE_SHORT:
      return "SMALLINT"s + appedn_unsigned(field);
    case MYSQL_TYPE_LONG:
      return " INTEGER"s + appedn_unsigned(field);
    case MYSQL_TYPE_FLOAT:
      return "FLOAT"s + append_length_dec(field);
    case MYSQL_TYPE_DOUBLE:
      return "DOUBLE"s + append_length_dec(field);
    case MYSQL_TYPE_LONGLONG:
      return "BIGINT"s + appedn_unsigned(field);
    case MYSQL_TYPE_INT24:
      return "MEDIUMINT"s + appedn_unsigned(field);

    case MYSQL_TYPE_TYPED_ARRAY:
    case MYSQL_TYPE_INVALID:
      return "INVALID";
    case MYSQL_TYPE_NULL:
      return "NULL";

    case MYSQL_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_TIME:
      return "TIME";
    case MYSQL_TYPE_DATETIME:
      return "DATETIME";
    case MYSQL_TYPE_YEAR:
      return "YEAR";
    case MYSQL_TYPE_NEWDATE:
      return "DATE";
    case MYSQL_TYPE_TIME2:
      return "TIME";
    case MYSQL_TYPE_TIMESTAMP2:
      return "TIMESTAMP";
    case MYSQL_TYPE_DATETIME2:
      return "DATETIME";
    case MYSQL_TYPE_BIT:
      return "BIT" + append_length(field);
    case MYSQL_TYPE_JSON:
      return "JSON";

    case MYSQL_TYPE_VARCHAR:
      return "VARCHAR" + append_length(field);
    case MYSQL_TYPE_SET:
      return "SET";
    case MYSQL_TYPE_ENUM:
      return "ENUM";
    case MYSQL_TYPE_VAR_STRING:
      return "VARCHAR"s + append_length(field);
    case MYSQL_TYPE_GEOMETRY:
      return "GEOMETRY";
    case MYSQL_TYPE_STRING:
      return "CHAR"s + append_length(field);

    case MYSQL_TYPE_TINY_BLOB:
      if (field->charsetnr == 63) return "TINYBLOB";
      return "TINYTEXT";
    case MYSQL_TYPE_MEDIUM_BLOB:
      if (field->charsetnr == 63) return "MEDIUMBLOB";
      return "MEDIUMTEXT";
    case MYSQL_TYPE_LONG_BLOB:
      return "BLOB";
    case MYSQL_TYPE_BLOB:
      if (field->charsetnr == 63) return "BLOB";
      return "TEXT";

    default:
      return "UNKNOWN";
  }

  return "UNKNOWN";
}

JsonType from_mysql_column_type(const MYSQL_FIELD *field) {
  switch (field->type) {
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
      return helper::JsonType::kNumeric;

    case MYSQL_TYPE_TYPED_ARRAY:
    case MYSQL_TYPE_INVALID:
    case MYSQL_TYPE_NULL:
      return helper::JsonType::kNull;

    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
      return helper::JsonType::kString;

    case MYSQL_TYPE_BIT:
      if (field->length != 1) return helper::JsonType::kBlob;
      [[fallthrough]];
    case MYSQL_TYPE_BOOL:
      return helper::JsonType::kBool;

    case MYSQL_TYPE_JSON:
      return helper::JsonType::kJson;

    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_STRING:
      if (field->charsetnr == 63 || IS_BLOB(field->flags))
        return helper::JsonType::kBlob;
      return helper::JsonType::kString;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      if (field->charsetnr == 63) return helper::JsonType::kBlob;
      return helper::JsonType::kString;
  }

  return helper::JsonType::kNull;
}

void remove_suffix_after(std::string_view &v, char c) {
  auto position = v.find(c);
  if (position != v.npos) v.remove_suffix(v.size() - position);
}

ColumnType from_mysql_txt_column_type(const char *t) {
  const auto &map = get_txt_type_mapping();
  std::string_view type{t, strlen(t)};
  remove_suffix_after(type, ' ');
  remove_suffix_after(type, '(');

  std::string type_string{type.begin(), type.end()};
  mysql_harness::lower(type_string);
  auto it = map.find(type_string);
  if (map.end() == it) return {};

  auto result = it->second;

  auto num = strstr(t, "(");
  if (num) {
    result.length = atoi(++num);
  }

  if (result.type_mysql == MYSQL_TYPE_BIT &&
      (result.length == 1 || result.length == 0))
    result.type_json = JsonType::kBool;

  return result;
}

uint64_t from_mysql_column_type_length(const char *) { return 0; }

std::string to_string(JsonType type) {
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
    case kBlob:
      return "blob";
  }
  return "null";
}

}  // namespace helper
