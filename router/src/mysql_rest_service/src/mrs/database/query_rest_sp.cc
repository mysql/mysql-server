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

#include "mrs/database/query_rest_sp.h"

#include "mrs/interface/http_result.h"
#include "mrs/json/json_template_factory.h"
#include "mrs/json/response_sp_json_template_nest.h"
#include "mrs/json/response_sp_json_template_unnest.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace mrs::json;
using namespace mrs::database::entry;

static MYSQL_FIELD *columns_find(const std::string &look_for, unsigned number,
                                 MYSQL_FIELD *fields) {
  for (unsigned i = 0; i < number; ++i) {
    if (look_for == fields[i].name) return &fields[i];
  }

  return nullptr;
}

static const Field *columns_find(const std::string &look_for,
                                 const std::vector<Field> &rs) {
  for (auto &f : rs) {
    if (look_for == f.bind_name) return &f;
  }

  return nullptr;
}

static void impl_columns_set(std::vector<helper::Column> &c,
                             const std::vector<Field> &rs, unsigned,
                             MYSQL_FIELD *fields) {
  c.resize(rs.size());
  int idx = 0;
  for (auto &i : c) {
    auto &f = fields[idx++];
    i = helper::Column(&f);
    auto column_def = columns_find(f.name, rs);
    i.name = column_def->name;
  }
}

static void impl_columns_set(std::vector<helper::Column> &c, unsigned number,
                             MYSQL_FIELD *fields) {
  c.resize(number);
  int idx = 0;
  for (auto &i : c) {
    i = helper::Column(&fields[idx++]);
  }
}

static const Field *columns_match(const std::vector<Field> &columns,
                                  unsigned number, MYSQL_FIELD *fields) {
  if (columns.size() != number) return nullptr;

  for (const auto &c : columns) {
    auto f = columns_find(c.bind_name, number, fields);
    if (f) {
      return &c;
    }
  }

  return nullptr;
}

QueryRestSP::QueryRestSP(JsonTemplateFactory *factory) : factory_{factory} {}

void QueryRestSP::columns_set(unsigned number, MYSQL_FIELD *fields) {
  {
    std::vector<Field> selected{};
    for (auto &c : rs_->input_parameters.fields) {
      if (c.mode == Field::modeIn) continue;
      selected.push_back(c);
    }
    if (columns_match(selected, number, fields)) {
      log_debug("Matched out-params");
      columns_items_type_ = rs_->input_parameters.name + "Out";
      impl_columns_set(columns_, selected, number, fields);
      return;
    }
  }

  for (auto &rs : rs_->results) {
    if (columns_match(rs.fields, number, fields)) {
      log_debug("Matched resultset with name %s", rs.name.c_str());
      columns_items_type_ = rs.name;
      impl_columns_set(columns_, rs.fields, number, fields);
      return;
    }
  }

  using namespace std::string_literals;
  log_debug("No match");
  columns_items_type_ = "items"s + std::to_string(resultset_);
  impl_columns_set(columns_, number, fields);
}

std::shared_ptr<JsonTemplate> QueryRestSP::create_template() {
  mrs::json::JsonTemplateFactory default_factory;
  mrs::database::JsonTemplateFactory *factory = &default_factory;
  using JsonTemplateType = mrs::database::JsonTemplateType;

  if (factory_) factory = factory_;

  return factory->create_template(JsonTemplateType::kObjectNested);
}

const char *QueryRestSP::get_sql_state() {
  if (!sqlstate_.has_value()) return nullptr;
  return sqlstate_.value().c_str();
}

void QueryRestSP::query_entries(
    MySQLSession *session, const std::string &schema, const std::string &object,
    const std::string &url, const std::string &ignore_column,
    const mysqlrouter::sqlstring &values, std::vector<enum_field_types> pt,
    const ResultSets &rs) {
  rs_ = &rs;
  items_started_ = false;
  items = 0;
  number_of_resultsets_ = 0;
  items_in_resultset_ = 0;
  ignore_column_ = ignore_column.c_str();
  query_ = {"CALL !.!(!)"};
  query_ << schema << object << values;
  url_ = url;
  has_out_params_ = !pt.empty();
  resultset_ = 0;

  response_template_ = create_template();
  response_template_->begin();

  prepare_and_execute(session, query_, pt);

  response_template_->finish();

  response = response_template_->get_result();
}

void QueryRestSP::on_row(const ResultRow &r) {
  response_template_->push_json_document(r, ignore_column_);
}

void QueryRestSP::on_metadata(unsigned int number, MYSQL_FIELD *fields) {
  log_debug("rs_->input_parameters.fields.size()=%i",
            (int)rs_->input_parameters.fields.size());

  log_debug("rs_->input_parameters.name=%s",
            rs_->input_parameters.name.c_str());

  int i = 0;
  for (auto &f : rs_->input_parameters.fields) {
    log_debug("rs_->input_parameters.fields[%i].bind_name:%s", i,
              f.bind_name.c_str());
    log_debug("rs_->input_parameters.fields[%i].name:%s", i, f.name.c_str());
    ++i;
  }

  log_debug("rs_->results.size()=%i", (int)rs_->results.size());

  int j = 0;
  for (auto &r : rs_->results) {
    log_debug("r[%i].name=%s", j, r.name.c_str());
    log_debug("r[%i].fields.size()=%i", j, (int)r.fields.size());

    i = 0;
    for (auto &f : r.fields) {
      log_debug("rs_->results[%i].fields[%i].bind_name:%s", j, i,
                f.bind_name.c_str());
      log_debug("rs_->results[%i].fields[%i].name:%s", j, i, f.name.c_str());
      ++i;
    }
    ++j;
  }
  columns_set(number, fields);

  if (number) {
    resultset_++;
    response_template_->begin_resultset(url_, columns_items_type_, columns_);
  }
}

}  // namespace database
}  // namespace mrs
