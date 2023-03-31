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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_QUERY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_QUERY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "mrs/database/entry/auth_user.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/entry/row_group_ownership.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/database/helper/query.h"

#include "mrs/json/response_json_template.h"

namespace mrs {
namespace database {

class QueryRestTable : private QueryLog {
 public:
  using Object = entry::Object;
  using ObjectField = entry::ObjectField;
  using UserId = entry::AuthUser::UserId;
  using UniversalId = entry::UniversalId;
  using RowGroupOwnership = entry::RowGroupOwnership;
  using RowUserOwnership = entry::RowUserOwnership;

  using VectorOfRowGroupOwnershp = std::vector<RowGroupOwnership>;

 public:
  virtual void query_entries(MySQLSession *session, const Object &object,
                             const ObjectFieldFilter &field_filter,
                             const uint32_t offset, const uint32_t limit,
                             const std::string &url, const std::string &primary,
                             const bool is_default_limit,
                             const RowUserOwnership &user_ownership = {},
                             UserId *user_id = nullptr,
                             const VectorOfRowGroupOwnershp &row_groups = {},
                             const std::set<UniversalId> &user_groups = {},
                             const std::string &query = {});

  std::string response;
  uint64_t items;

 private:
  json::ResponseJsonTemplate serializer_;
  mysqlrouter::sqlstring where_;

  void on_row(const Row &r) override;

  const mysqlrouter::sqlstring &build_where(
      const RowUserOwnership &row_user, UserId *user_id,
      const std::vector<RowGroupOwnership> &row_groups,
      const std::set<UniversalId> &user_groups);

  void build_query(const Object &object, const ObjectFieldFilter &field_filter,
                   const uint32_t offset, const uint32_t limit,
                   const std::string &url, const std::string &primary,
                   const RowUserOwnership &user_row, UserId *user_id,
                   const std::vector<RowGroupOwnership> &row_groups,
                   const std::set<UniversalId> &user_groups,
                   const std::string &query);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_QUERY_H_
