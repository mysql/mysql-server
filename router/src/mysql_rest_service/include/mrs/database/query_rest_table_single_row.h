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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "helper/mysql_column.h"
#include "mrs/database/json_mapper/select.h"
#include "mrs/database/query_rest_table.h"

namespace mrs {
namespace database {

class QueryRestTableSingleRow : public QueryRestTable {
 public:
  using Object = entry::Object;
  using ObjectField = entry::ObjectField;
  using PrimaryKeyColumnValues = mrs::database::PrimaryKeyColumnValues;
  using ObjectFieldFilter = dv::ObjectFieldFilter;

 public:
  explicit QueryRestTableSingleRow(
      const JsonTemplateFactory *factory = nullptr,
      bool encode_bigints_as_string = false, const bool include_links = true,
      const RowLockType lock_rows = RowLockType::NONE,
      uint64_t max_execution_time_ms = 0);

  virtual void query_entry(MySQLSession *session,
                           std::shared_ptr<database::entry::Object> object,
                           const PrimaryKeyColumnValues &pk,
                           const dv::ObjectFieldFilter &field_filter,
                           const std::string &url_route,
                           const ObjectRowOwnership &row_ownership,
                           const FilterObjectGenerator &fog = {},
                           const bool compute_etag = false,
                           std::function<std::string()> commit = {},
                           const bool fetch_any_owner = false);

  bool is_owned() const { return is_owned_; }

  static std::string make_short_response(const PrimaryKeyColumnValues &pk,
                                         const std::string &response_gtid);

 private:
  bool is_owned_ = true;
  RowLockType lock_rows_ = RowLockType::NONE;
  std::function<std::string()> commit_;

  void on_row(const ResultRow &r) override;
  void build_query(const dv::ObjectFieldFilter &field_filter,
                   const std::string &url_route,
                   const ObjectRowOwnership &row_ownership,
                   const PrimaryKeyColumnValues &pk, bool fetch_any_owner,
                   const FilterObjectGenerator &fog);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
