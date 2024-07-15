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

#include "mrs/database/query_entry_group_row_security.h"

namespace mrs {
namespace database {

bool QueryEntryGroupRowSecurity::query_group_row_security(
    MySQLSession *session, entry::UniversalId db_object_id) {
  row_group_security_.clear();
  query_ = {
      "SELECT group_hierarchy_type_id, row_group_ownership_column, level, "
      "match_level + 0 FROM "
      "mysql_rest_service_metadata.mrs_db_object_row_group_security WHERE "
      "db_object_id=?"};
  query_ << db_object_id;
  execute(session);

  return true;
}

QueryEntryGroupRowSecurity::RowGroupsSecurity &
QueryEntryGroupRowSecurity::get_result() {
  return row_group_security_;
}

void QueryEntryGroupRowSecurity::on_row(const ResultRow &row) {
  if (row.size() < 1) return;

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  auto converter = [](MatchLevel *out_ml, const char *db_value) {
    *out_ml = static_cast<MatchLevel>(atoi(db_value));
  };

  auto &entry = row_group_security_.emplace_back();

  mysql_row.unserialize_with_converter(&entry.hierarhy_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.row_group_ownership_column);
  mysql_row.unserialize(&entry.level);
  mysql_row.unserialize_with_converter(&entry.match, converter);
}

}  // namespace database
}  // namespace mrs
