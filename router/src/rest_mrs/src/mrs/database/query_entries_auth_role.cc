/*
 Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

void QueryEntriesAuthRole::query(MySQLSession *session,
                                 const uint64_t user_id) {
  query_ = {
      "SELECT id, caption, derived_from_role_id, specific_to_service_id "
      " FROM mysql_rest_service_metadata.user_has_role as h "
      " JOIN mysql_rest_service_metadata.auth_role as r ON r.id=h.auth_role_id "
      " WHERE h.auth_user_id = ?"};

  query_ << user_id;
  Query::query(session);
}

void QueryEntriesAuthRole::on_row(const mrs::database::Query::Row &r) {
  auto &role = result.emplace_back();
  helper::MySQLRow mysql_row(r);

  mysql_row.unserialize(&role.id);
  mysql_row.unserialize(&role.caption);
  mysql_row.unserialize(&role.derived_from_role_id);
  mysql_row.unserialize(&role.specific_to_service_id);
}

}  // namespace database
}  // namespace mrs
