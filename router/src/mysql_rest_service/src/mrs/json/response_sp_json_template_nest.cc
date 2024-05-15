/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/json/response_sp_json_template_nest.h"

#include <limits>

#include "mysqlrouter/base64.h"

namespace mrs {
namespace json {

std::string ResponseSpJsonTemplateNest::get_result() {
  return serializer_.get_result();
}

void ResponseSpJsonTemplateNest::flush() { serializer_.flush(); }

void ResponseSpJsonTemplateNest::begin_resultset(
    const std::string &url, const std::string &items_name,
    const std::vector<helper::Column> &columns) {
  end_resultset();
  json_root_items_object_ = json_root_items_->add_object();
  json_root_items_object_->member_add_value("type", items_name);
  json_root_items_object_items_ =
      json_root_items_object_->member_add_array("items");

  url_ = url;
  columns_ = columns;
}

void ResponseSpJsonTemplateNest::begin_resultset(
    uint64_t, uint64_t, bool, const std::string &,
    const std::vector<helper::Column> &) {
  assert(false && "not implemented in sp");
}

void ResponseSpJsonTemplateNest::end_resultset() {
  json_root_items_object_items_ = JsonSerializer::Array();
  if (json_root_items_object_.is_usable()) {
    auto m = json_root_items_object_->member_add_object("_metadata");
    auto a = m->member_add_array("columns");
    for (auto &c : columns_) {
      auto oc = a->add_object();
      oc->member_add_value("name", c.name);
      oc->member_add_value("type", c.type_txt);
    }
  }
  json_root_items_object_ = JsonSerializer::Object();
}

void ResponseSpJsonTemplateNest::begin() {
  json_root_ = serializer_.add_object();
  pushed_documents_ = 0;
  json_root_items_ = serializer_.member_add_array("items");
}

void ResponseSpJsonTemplateNest::finish() {
  end_resultset();

  json_root_items_object_items_ = JsonSerializer::Array();
  json_root_items_object_ = JsonSerializer::Object();
  json_root_items_ = JsonSerializer::Array();
  json_root_ = JsonSerializer::Object();
}

bool ResponseSpJsonTemplateNest::push_json_document(const ResultRow &values,
                                                    const char *ignore_column) {
  auto &columns = columns_;
  assert(values.size() == columns.size());
  auto obj = json_root_items_object_items_->add_object();

  for (size_t idx = 0; idx < values.size(); ++idx) {
    if (ignore_column && columns[idx].name == ignore_column) {
      ignore_column = nullptr;
      continue;
    }

    auto type_json = columns[idx].type_json;

    if (encode_bigints_as_string_ && type_json == helper::JsonType::kNumeric) {
      if (should_encode_numeric_as_string(columns[idx].type)) {
        serializer_.member_add_value(columns[idx].name, values[idx],
                                     helper::JsonType::kString);
        continue;
      }
    }

    switch (type_json) {
      case helper::JsonType::kBool:
        serializer_.member_add_value(
            columns[idx].name,
            (*reinterpret_cast<const uint8_t *>(values[idx]) != 0) ? "true"
                                                                   : "false",
            type_json);
        break;
      case helper::JsonType::kBlob:
        serializer_.member_add_value(
            columns[idx].name,
            Base64::encode(
                std::string_view(values[idx], values.get_data_size(idx)))
                .c_str(),
            type_json);
        break;
      default:
        serializer_.member_add_value(columns[idx].name, values[idx], type_json);
    }
  }

  return true;
}

bool ResponseSpJsonTemplateNest::push_json_document(const char *) {
  assert(false && "not implemented");
  return true;
}

}  // namespace json
}  // namespace mrs
