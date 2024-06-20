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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "helper/mysql_column.h"
#include "mrs/database/duality_view/select.h"
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
  explicit QueryRestTableSingleRow(const JsonTemplateFactory *factory = nullptr,
                                   bool encode_bigints_as_string = false,
                                   const bool include_links = true);

  virtual void query_entry(MySQLSession *session,
                           std::shared_ptr<database::entry::Object> object,
                           const PrimaryKeyColumnValues &pk,
                           const dv::ObjectFieldFilter &field_filter,
                           const std::string &url_route,
                           const ObjectRowOwnership &row_ownership,
                           const bool compute_etag = false,
                           const std::string &metadata_gtid = {},
                           const bool fetch_any_owner = false);

  bool is_owned() const { return is_owned_; }

 private:
  std::string metadata_gtid_{};
  bool is_owned_ = true;

  void on_row(const ResultRow &r) override;
  void build_query(const dv::ObjectFieldFilter &field_filter,
                   const std::string &url_route,
                   const ObjectRowOwnership &row_ownership,
                   const PrimaryKeyColumnValues &pk, bool fetch_any_owner);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
