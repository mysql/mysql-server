/*
 Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_sp.h"

namespace mrs {
namespace database {

void QueryRestSP::query_entries(
    MySQLSession *session, const std::string &schema, const std::string &object,
    const std::string &url, const std::string &ignore_column,
    const mysqlrouter::sqlstring &values, std::vector<enum_field_types> pt) {
  items_started_ = false;
  items = 0;
  number_of_resultsets_ = 0;
  items_in_resultset_ = 0;
  ignore_column_ = ignore_column.c_str();
  query_ = {"CALL !.!(!)"};
  query_ << schema << object << values;
  url_ = url;

  has_out_params_ = !pt.empty();

  prepare_and_execute(session, query_, pt);

  flush(true);
  response_template.end();

  response = response_template.get_result();
}

bool QueryRestSP::flush(const bool is_last) {
  if (0 == items_in_resultset_ && !(is_last && !items_started_)) return true;
  if (items_in_resultset_ > 1) return false;

  items_started_ = true;
  if (is_last && has_out_params_) {
    response_template.begin(url_, "itemsOut");
    push_cached();
    return false;
  }

  using namespace std::string_literals;
  response_template.begin(
      url_, "items"s + (number_of_resultsets_ > 1
                            ? std::to_string(number_of_resultsets_)
                            : ""s));

  push_cached();

  return false;
}

void QueryRestSP::push_cached() {
  if (items_in_resultset_) {
    Row r;
    r.reserve(flush_copy_.size());
    for (auto &item : flush_copy_) {
      r.push_back(item ? item.value().c_str() : nullptr);
    }
    push_to_document(r);
  }
}

void QueryRestSP::on_row(const ResultRow &r) {
  auto should_cache = flush();
  ++items;
  ++items_in_resultset_;

  if (should_cache) {
    flush_copy_.clear();
    flush_copy_.reserve(r.size());

    for (auto item : r) {
      if (item)
        flush_copy_.emplace_back(item);
      else
        flush_copy_.emplace_back();
    }
    return;
  }
  push_to_document(r);
}

void QueryRestSP::push_to_document(const ResultRow &r) {
  response_template.push_json_document(r, columns_, ignore_column_);
}

void QueryRestSP::on_metadata(unsigned int number, MYSQL_FIELD *fields) {
  if (number) {
    flush();
    ++number_of_resultsets_;
    items_in_resultset_ = 0;
    columns_.clear();
    for (unsigned int i = 0; i < number; ++i) {
      columns_.emplace_back(&fields[i]);
    }
  }
}

}  // namespace database
}  // namespace mrs
