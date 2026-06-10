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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_NEST_WITHOUT_OUTPARAMS_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_NEST_WITHOUT_OUTPARAMS_H_

#include <string>
#include <vector>

#include "helper/json/serializer_to_text.h"
#include "helper/mysql_column.h"
#include "mrs/json/json_template_nest.h"
#include "mysqlrouter/mysql_session.h"

namespace mrs {
namespace json {

class JsonTemplateNestWithoutOutParameters : public JsonTemplateNest {
 public:
  using JsonSerializer = helper::json::SerializerToText;
  using ResultRow = mysqlrouter::MySQLSession::ResultRow;

 public:
  explicit JsonTemplateNestWithoutOutParameters(
      const bool encode_bigints_as_string = false);

  void begin_resultset(const std::string &url, const std::string &items_name,
                       const std::vector<helper::Column> &columns) override;
  bool push_row(const ResultRow &values,
                const char *ignore_column = nullptr) override;
  void end_resultset(const std::optional<bool> &has_more = {}) override;
  void begin() override;

 private:
  bool parameter_resultset_{false};
  bool block_push_json_document_{false};
};

}  // namespace json
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_NEST_WITHOUT_OUTPARAMS_H_
