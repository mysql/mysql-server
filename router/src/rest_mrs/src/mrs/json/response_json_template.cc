/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/json/response_json_template.h"

#include <limits>

namespace mrs {
namespace json {

std::string ResponseJsonTemplate::get_result() {
  return serializer_.get_result();
}

void ResponseJsonTemplate::begin(const std::string &url) {
  assert(began_ == false);
  // Initialize data to be supplied to the template
  offset_ = 0;
  limit_ = std::numeric_limits<decltype(limit_)>::max();
  is_default_limit_ = true;
  url_ = url;

  // Start serialization, initialize internal state
  began_ = true;
  has_more_ = false;
  pushed_documents_ = 0;
  json_root_ = serializer_.add_object();
  json_root_items_ = serializer_.member_add_array("items");
}

void ResponseJsonTemplate::begin(uint32_t offset, uint32_t limit,
                                 bool is_default_limit,
                                 const std::string &url) {
  assert(began_ == false);
  // Initialize data to be supplied to the template
  offset_ = offset;
  limit_ = limit;
  is_default_limit_ = is_default_limit;
  url_ = url;

  // Start serialization, initialize internal state
  began_ = true;
  has_more_ = false;
  pushed_documents_ = 0;
  json_root_ = serializer_.add_object();
  json_root_items_ = serializer_.member_add_array("items");
}

void ResponseJsonTemplate::end() {
  assert(began_ == true);
  json_root_items_ = JsonSerializer::Array();
  json_root_->member_add_value("limit", limit_);
  json_root_->member_add_value("offset", offset_);
  json_root_->member_add_value("hasMore", has_more_,
                               helper::ColumnJsonTypes::kBool);
  json_root_->member_add_value("count", std::min(limit_, pushed_documents_));
  {
    auto array_links = serializer_.member_add_array("links");
    array_links->add_object()
        ->member_add_value("rel", "self")
        .member_add_value("href", url_ + "/");

    if (has_more_) {
      auto url_next =
          url_ + "/?offset=" + std::to_string(offset_ + limit_) +
          (is_default_limit_ ? "" : "&limit=" + std::to_string(limit_));
      array_links->add_object()
          ->member_add_value("rel", "next")
          .member_add_value("href", url_next);
    }

    if (offset_) {
      auto url_prev =
          url_ + "/?offset=" +
          std::to_string(offset_ >= limit_ ? offset_ - limit_ : 0) +
          (is_default_limit_ ? "" : "&limit=" + std::to_string(limit_));
      auto url_first =
          url_ + (is_default_limit_ ? "" : "/?limit=" + std::to_string(limit_));
      array_links->add_object()
          ->member_add_value("rel", "prev")
          .member_add_value("href", url_prev);
      array_links->add_object()
          ->member_add_value("rel", "first")
          .member_add_value("href", url_first);
    }
  }
  json_root_ = JsonSerializer::Object();
  began_ = false;
}

bool ResponseJsonTemplate::push_json_document(
    const std::vector<const char *> &values,
    const std::vector<helper::Column> &columns, const char *ignore_column) {
  assert(began_ == true);
  assert(values.size() == columns.size());
  if (!count_check_if_push_is_allowed()) return false;

  auto obj = serializer_.add_object();

  for (size_t idx = 0; idx < values.size(); ++idx) {
    if (ignore_column && columns[idx].name == ignore_column) {
      ignore_column = nullptr;
      continue;
    }
    serializer_.member_add_value(columns[idx].name, values[idx],
                                 columns[idx].type_json);
  }

  return true;
}

bool ResponseJsonTemplate::push_json_document(const char *doc) {
  assert(began_ == true);
  if (!count_check_if_push_is_allowed()) return false;

  serializer_.add_value(doc, helper::ColumnJsonTypes::kJson);

  return true;
}

bool ResponseJsonTemplate::count_check_if_push_is_allowed() {
  if (pushed_documents_ >= limit_ &&
      limit_ != std::numeric_limits<decltype(limit_)>::max()) {
    has_more_ = true;
    return false;
  }

  ++pushed_documents_;

  return true;
}

}  // namespace json
}  // namespace mrs
