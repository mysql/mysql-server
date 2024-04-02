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

#include "mrs/database/helper/query_table_columns.h"

#include "mysql/harness/logging/logging.h"

#include "helper/string/contains.h"

#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

void QueryTableColumns::query_entries(MySQLSession *session,
                                      const std::string &schema,
                                      const std::string &object) {
  columns.clear();

  query_ = "show columns from !.!;";
  query_ << schema << object;

  try {
    query(session,
          "SET @@show_gipk_in_create_table_and_information_schema=OFF ");
  } catch (const MySQLSession::Error &e) {
    if (ER_UNKNOWN_SYSTEM_VARIABLE != e.code()) throw;
  }
  execute(session);
}

void QueryTableColumns::on_row(const ResultRow &r) {
  using namespace std::string_literals;
  log_debug("Column %s %s %s (is_primary %s)", r[0], r[1], r[3],
            ("PRI"s == r[3] ? "yes" : "no"));
  columns.emplace_back(r[0], r[1], "PRI"s == r[3],
                       helper::contains(r[5] ? r[5] : "", "auto_increment"));
}

}  // namespace database
}  // namespace mrs
