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

#include "mrs/database/query_entries_auth_role.h"
#include "helper/mysql_row.h"

#include <stdexcept>

namespace mrs {
namespace database {

void QueryEntriesAuthRole::query_role(MySQLSession *session,
                                      const entry::AuthUser::UserId user_id) {
  query_ = {
      "SELECT id, caption, derived_from_role_id, specific_to_service_id "
      " FROM mysql_rest_service_metadata.mrs_user_has_role as h "
      " JOIN mysql_rest_service_metadata.mrs_role as r ON r.id=h.role_id "
      " WHERE h.user_id = ?"};

  query_ << to_sqlstring(user_id);
  Query::execute(session);
}

void QueryEntriesAuthRole::on_row(const ResultRow &r) {
  auto &role = result.emplace_back();
  helper::MySQLRow mysql_row(r, metadata_, num_of_metadata_);

  mysql_row.unserialize_with_converter(&role.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&role.caption);
  mysql_row.unserialize_with_converter(&role.derived_from_role_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&role.specific_to_service_id,
                                       entry::UniversalId::from_raw);
}

}  // namespace database
}  // namespace mrs
