/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "mrs/json/json_template_nest.h"

#include <limits>

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace json {

JsonTemplateNest::JsonTemplateNest(const bool encode_bigints_as_string)
    : encode_bigints_as_string_{encode_bigints_as_string} {
  log_debug("ResponseSpJsonTemplateNest");
}

std::string JsonTemplateNest::get_result() { return serializer_.get_result(); }

void JsonTemplateNest::flush() { serializer_.flush(); }

void JsonTemplateNest::begin_resultset(
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

void JsonTemplateNest::begin_resultset_with_limits(
    uint64_t, uint64_t, bool, const std::string &,
    const std::vector<helper::Column> &) {
  assert(false && "not implemented in sp");
}

void JsonTemplateNest::end_resultset(
    [[maybe_unused]] const std::optional<bool> &has_more) {
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

void JsonTemplateNest::begin() {
  json_root_ = serializer_.add_object();
  pushed_documents_ = 0;
  json_root_items_ = serializer_.member_add_array("items");
}

void JsonTemplateNest::finish(const CustomMetadata &custom_metadata) {
  end_resultset();

  json_root_items_ = JsonSerializer::Array();
  if (json_root_.is_usable()) {
    if (!custom_metadata.empty()) {
      auto m = json_root_->member_add_object("_metadata");
      for (const auto &md : custom_metadata) {
        m->member_add_value(md.first, md.second);
      }
    }
  }

  json_root_items_object_items_ = JsonSerializer::Array();
  json_root_items_object_ = JsonSerializer::Object();
  json_root_ = JsonSerializer::Object();
}

bool JsonTemplateNest::push_row(const ResultRow &values,
                                const char *ignore_column) {
  auto obj = json_root_items_object_items_->add_object();
  return push_row_impl(values, ignore_column);
}

bool JsonTemplateNest::push_row_impl(const ResultRow &values,
                                     const char *ignore_column) {
  auto &columns = columns_;
  assert(values.size() == columns.size());

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

    if (!values[idx]) {
      serializer_.member_add_value(columns[idx].name.c_str(), nullptr, 0);
      continue;
    }

    if (columns[idx].type == MYSQL_TYPE_VECTOR) {
      auto arr = serializer_.member_add_array(columns[idx].name.c_str());
      auto float_ptr = reinterpret_cast<const float *>(values[idx]);
      auto no_of_elements = values.get_data_size(idx) / sizeof(float);
      for (uint32_t i = 0; i < no_of_elements; ++i) {
        (*arr) << float_ptr[i];
      }
      continue;
    }

    switch (type_json) {
      case helper::JsonType::kBool: {
        const char *value = values[idx];
        if (!columns[idx].is_bound) {
          value = ((*value != 0) ? "true" : "false");
        } else {
          value = atoi(value) ? "true" : "false";
        }
        serializer_.member_add_value(columns[idx].name, value, type_json);
      } break;
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

bool JsonTemplateNest::push_json_document(const char *) {
  assert(false && "not implemented");
  return true;
}

}  // namespace json
}  // namespace mrs
