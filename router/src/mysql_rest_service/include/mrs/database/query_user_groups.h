/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_USER_GROUPS_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_USER_GROUPS_H_

#include <set>
#include <utility>

#include "mrs/database/entry/auth_user.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryUserGroups : private Query {
 public:
  using Set = std::set<entry::UniversalId>;

 public:
  virtual void query_groups(MySQLSession *session,
                            const entry::AuthUser::UserId &user_id,
                            Set *out_group_ids) {
    out_group_ids->clear();
    set_ = out_group_ids;
    query_.reset(
        "SELECT user_group_id FROM "
        "mysql_rest_service_metadata.mrs_user_has_group "
        "WHERE user_id=?;");
    query_ << to_sqlstring(user_id);
    execute(session);
  }

 private:
  void on_row(const ResultRow &r) override {
    if (r[0]) set_->insert(entry::UniversalId::from_cstr(r[0], 16));
  }
  Set *set_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_USER_GROUPS_H_
