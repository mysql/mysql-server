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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SCHEMA_VALIDATOR_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SCHEMA_VALIDATOR_H_

#include <string>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"

namespace helper {
namespace json {

inline bool validate_json_with_schema(
    const std::string &json, const std::string &schema,
    std::string *error_description = nullptr) {
  using namespace std::string_literals;
  auto get_pointer_string =
      [](const rapidjson::Pointer &pointer) -> std::string {
    rapidjson::StringBuffer buff;
    pointer.StringifyUriFragment(buff);
    return {buff.GetString(), buff.GetSize()};
  };

  rapidjson::Document rjschema;
  rapidjson::Document rjjson;

  rjschema.Parse(schema.c_str(), schema.length());
  rjjson.Parse(json.c_str(), json.length());

  if (rjschema.HasParseError()) {
    *error_description = "Validation schema, parsing error: "s +
                         std::to_string(rjschema.GetParseError());
    return false;
  }

  if (rjjson.HasParseError()) {
    *error_description = "Json object, parsing error: "s +
                         std::to_string(rjjson.GetParseError());
    return false;
  }

  rapidjson::SchemaDocument schema_document{rjschema};
  rapidjson::SchemaValidator validator{schema_document};

  if (!validator.IsValid()) {
    if (error_description) {
      *error_description = "Validator is invalid.";
    }
    return false;
  }

  if (rjjson.Accept(validator)) return true;

  if (error_description) {
    using namespace std::string_literals;
    *error_description =
        "JSON validation location "s +
        get_pointer_string(validator.GetInvalidDocumentPointer()) +
        " failed requirement: '" + validator.GetInvalidSchemaKeyword() +
        "' at meta schema location '" +
        get_pointer_string(validator.GetInvalidSchemaPointer()) + "'";
  }

  return false;
}

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SCHEMA_VALIDATOR_H_
