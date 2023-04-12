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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_REST_SP_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_REST_SP_H_

#include <optional>
#include <string>
#include <vector>

#include "helper/mysql_column.h"
#include "mrs/database/helper/query.h"
#include "mrs/json/response_json_template.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class QueryRestSP : private QueryLog {
  using Row = Query::Row;

 public:
  virtual void query_entries(MySQLSession *session, const std::string &schema,
                             const std::string &object, const std::string &url,
                             const std::string &ignore_column,
                             const mysqlrouter::sqlstring &values = {},
                             std::vector<enum_field_types> pt = {});

  std::string response;
  uint64_t items;

 private:
  bool has_out_params_;
  uint64_t items_in_resultset_;
  uint64_t number_of_resultsets_;
  json::ResponseJsonTemplate response_template;
  std::vector<helper::Column> columns_;
  const char *ignore_column_{nullptr};
  std::vector<std::optional<std::string>> flush_copy_;
  std::string url_;

  bool flush(const bool is_last = false);
  void push_cached();

  void on_row(const Row &r) override;
  void on_metadata(unsigned int number, MYSQL_FIELD *fields) override;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_REST_SP_H_
