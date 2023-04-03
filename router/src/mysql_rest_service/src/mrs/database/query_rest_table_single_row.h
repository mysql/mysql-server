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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryRestTableSingleRow : private QueryLog {
 public:
  using Object = entry::Object;
  using ObjectField = entry::ObjectField;

 public:
  virtual void query_entries(MySQLSession *session,
                             std::shared_ptr<database::entry::Object> object,
                             const ObjectFieldFilter &field_filter,
                             const std::string &primary_key,
                             const mysqlrouter::sqlstring &pri_value,
                             const std::string &url_route);

  virtual void query_last_inserted(
      MySQLSession *session, std::shared_ptr<database::entry::Object> object,
      const ObjectFieldFilter &field_filter, const std::string &primary_key,
      const std::string &url_route);

  std::string response;
  uint64_t items;

 private:
  void on_row(const Row &r) override;
  void build_query(std::shared_ptr<database::entry::Object> object,
                   const ObjectFieldFilter &field_filter,
                   const std::string &primary_key,
                   const mysqlrouter::sqlstring &pri_value,
                   const std::string &url_route);
  void build_query_last_inserted(
      std::shared_ptr<database::entry::Object> object,
      const ObjectFieldFilter &field_filter, const std::string &primary_key,
      const std::string &url_route);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_FETCH_ONE_H_
