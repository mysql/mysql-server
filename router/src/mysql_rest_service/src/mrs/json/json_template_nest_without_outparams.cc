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

#include "mrs/json/json_template_nest_without_outparams.h"

#include <limits>

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace json {

JsonTemplateNestWithoutOutParameters::JsonTemplateNestWithoutOutParameters(
    const bool encode_bigints_as_string)
    : JsonTemplateNest{encode_bigints_as_string} {
  log_debug("ResponseSpJsonTemplateNest");
}

void JsonTemplateNestWithoutOutParameters::begin_resultset(
    const std::string &url, const std::string &items_name,
    const std::vector<helper::Column> &columns) {
  // The resultsets after blocking, are ignored.
  // Properly configured MRS object should generate
  // resultset-s before out-parameters.
  //
  // The blocking state should be achieved after
  // parameter resultset, after it we ignore everything.
  if (block_push_json_document_) return;

  bool is_parameters_resultset = columns.size() && columns[0].is_bound;

  if (!is_parameters_resultset) {
    JsonTemplateNest::begin_resultset(url, items_name, columns);
    return;
  }

  end_resultset();
  columns_ = columns;
  url_ = url;
  parameter_resultset_ = is_parameters_resultset;

  // Clearing those objects will close them in serializer output,
  // thus for:
  //  'object' it will serialize  '}'
  //  'array' it will serialize  ']'
  json_root_items_object_items_ = JsonSerializer::Array();
  json_root_items_object_ = JsonSerializer::Object();
  json_root_items_ = JsonSerializer::Array();
}

void JsonTemplateNestWithoutOutParameters::end_resultset(
    [[maybe_unused]] const std::optional<bool> &has_more) {
  if (block_push_json_document_) return;

  if (parameter_resultset_) {
    block_push_json_document_ = true;
    return;
  }

  JsonTemplateNest::end_resultset();
}

void JsonTemplateNestWithoutOutParameters::begin() {
  json_root_ = serializer_.add_object();
  pushed_documents_ = 0;
  block_push_json_document_ = false;
  parameter_resultset_ = false;
  json_root_items_ = serializer_.member_add_array("resultSets");
}

bool JsonTemplateNestWithoutOutParameters::push_row(const ResultRow &values,
                                                    const char *ignore_column) {
  if (block_push_json_document_) return true;

  auto obj = parameter_resultset_
                 ? serializer_.member_add_object("outParameters")
                 : json_root_items_object_items_->add_object();
  return push_row_impl(values, ignore_column);
}

}  // namespace json
}  // namespace mrs
