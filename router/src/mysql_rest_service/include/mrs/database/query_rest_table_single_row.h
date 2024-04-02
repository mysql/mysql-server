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
#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryRestTableSingleRow : private QueryLog {
 public:
  using Object = entry::Object;
  using ObjectField = entry::ObjectField;
  using PrimaryKeyColumnValues = mrs::database::PrimaryKeyColumnValues;

 public:
  explicit QueryRestTableSingleRow(bool encode_bigints_as_string = false,
                                   const bool include_links = true);

  virtual void query_entries(MySQLSession *session,
                             std::shared_ptr<database::entry::Object> object,
                             const ObjectFieldFilter &field_filter,
                             const PrimaryKeyColumnValues &pk,
                             const std::string &url_route,
                             bool compute_etag = false,
                             const std::string &metadata_gtid = {});

  std::string response;
  uint64_t items;

 private:
  std::shared_ptr<database::entry::Object> object_;
  const ObjectFieldFilter *field_filter_{nullptr};
  bool compute_etag_{false};
  std::string metadata_gtid_{};
  bool encode_bigints_as_string_;
  bool include_links_;

  void on_row(const ResultRow &r) override;
  void build_query(std::shared_ptr<database::entry::Object> object,
                   const PrimaryKeyColumnValues &pk,
                   const std::string &url_route);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
