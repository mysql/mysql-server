/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <stdexcept>

#include "mrs/database/query_router_info.h"

#include "mysqld_error.h"

namespace mrs {
namespace database {

std::optional<uint64_t> QueryRouterInfo::find_existing_router_instances(
    MySQLSession *session, const std::string &router_name,
    const std::string &address) {
  try {
    mysqlrouter::sqlstring sql_query{
        "SELECT `id` FROM mysql_rest_service_metadata.router WHERE"
        " router_name = ? AND address = ?"};
    sql_query << router_name << address;
    query(session, sql_query);
  } catch (const mysqlrouter::MySQLSession::Error &error) {
    if (error.code() != ER_NO_SUCH_TABLE &&
        error.code() != ER_TABLEACCESS_DENIED_ERROR) {
      throw;
    }
  }
  return id_;
}

void QueryRouterInfo::on_row(const ResultRow &r) {
  if (r.size() != 1) {
    throw std::runtime_error(
        "Could not fetch router information from "
        "`mysql_rest_service_metadata`.`router`");
  }

  try {
    id_ = std::stoi(r[0]);
  } catch (const std::exception &) {
    id_ = std::nullopt;
  }
}

}  // namespace database
}  // namespace mrs
