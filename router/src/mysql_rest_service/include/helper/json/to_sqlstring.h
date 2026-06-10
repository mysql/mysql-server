/*  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_

#include <sstream>
#include <string>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>

#include "helper/json/rapid_json_iterator.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/to_string.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/entry/object.h"

#include "mysqlrouter/utils_sqlstring.h"

namespace helper {
namespace json {

template <typename Stream, typename Value>
Stream &to_stream(Stream &stream, const rapidjson::Value &v,
                  const Value &k_true, const Value &k_false) {
  if (v.IsNull()) {
    stream << nullptr;
  } else if (v.IsBool()) {
    stream << (v.GetBool() ? k_true : k_false);
  } else if (v.IsString()) {
    stream << v.GetString();
  } else if (v.IsUint()) {
    stream << v.GetUint();
  } else if (v.IsInt()) {
    stream << v.GetInt();
  } else if (v.IsUint64()) {
    stream << v.GetUint64();
  } else if (v.IsInt64()) {
    stream << v.GetInt64();
  } else if (v.IsFloat()) {
    stream << v.GetFloat();
  } else if (v.IsDouble()) {
    stream << v.GetDouble();
  } else if (v.IsArray() || v.IsObject()) {
    stream << to_string(v);
  } else {
    using namespace std::string_literals;
    throw std::runtime_error(
        "JSON value to SQLString, received unsupported type:"s +
        std::to_string(v.GetType()) + ".");
  }

  return stream;
}

namespace sql {

// To not keep this function in the same namespace as to_string
mysqlrouter::sqlstring &operator<<(mysqlrouter::sqlstring &sql,
                                   const rapidjson::Value &v);

mysqlrouter::sqlstring &operator<<(
    mysqlrouter::sqlstring &sql,
    const std::pair<rapidjson::Value *, mrs::database::entry::ColumnType> &v);

}  // namespace sql
}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_TO_SQLSTRING_H_
