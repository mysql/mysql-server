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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_TEXT_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_TEXT_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <string>

namespace helper {
namespace json {

template <typename T>
using Writer = rapidjson::Writer<T>;

template <template <typename> typename Writer = Writer, typename JsonValue>
void rapid_json_to_text(JsonValue *json_value, std::string &text_value) {
  rapidjson::StringBuffer json_buf;
  {
    Writer<rapidjson::StringBuffer> json_writer(json_buf);

    json_value->Accept(json_writer);
  }

  text_value.assign(json_buf.GetString(), json_buf.GetLength());
}

template <template <typename> typename Writer = Writer, typename JsonValue>
void append_rapid_json_to_text(JsonValue *json_value, std::string &text_value) {
  rapidjson::StringBuffer json_buf;
  {
    Writer<rapidjson::StringBuffer> json_writer(json_buf);

    json_value->Accept(json_writer);
  }

  text_value.append(json_buf.GetString(), json_buf.GetLength());
}

inline std::string to_string(const rapidjson::Value &value) {
  std::string res;
  append_rapid_json_to_text(&value, res);
  return res;
}

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_TEXT_H_
