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

#include "mysql/harness/utility/string.h"

#include "helper/mysql_row.h"
#include "mrs/database/query_entries_audit_log.h"

namespace mrs {
namespace database {

uint64_t QueryAuditLogEntries::query_entries(
    MySQLSession *session, const std::vector<std::string> &allowed_tables,
    const uint64_t audit_log_id) {
  max_id_ = audit_log_id;
  fetch_entries_ = true;
  build_query(allowed_tables, audit_log_id, false);
  execute(session);

  return max_id_;
}

uint64_t QueryAuditLogEntries::count_entries(
    MySQLSession *session, const std::vector<std::string> &allowed_tables,
    const uint64_t audit_log_id) {
  fetch_entries_ = false;
  no_of_entries_ = 0;
  build_query(allowed_tables, audit_log_id, true);
  execute(session);
  return no_of_entries_;
}

void QueryAuditLogEntries::build_query(
    const std::vector<std::string> &allowed_tables, const uint64_t audit_log_id,
    bool count_entries) {
  static mysqlrouter::sqlstring columns{
      count_entries ? "id,dml_type,table_name,old_row_id, new_row_id"
                    : "count(*)"};
  query_ = {
      "SELECT ! FROM "
      "mysql_rest_service_metadata.audit_log WHERE ID > ? AND table_name in "
      "(?) ORDER BY id"};

  query_ << columns;
  query_ << audit_log_id;
  query_ << allowed_tables;
}

void QueryAuditLogEntries::on_row(const ResultRow &row) {
  if (!fetch_entries_) {
    on_row_count(row);
    return;
  }
  on_row_entries(row);
}

void QueryAuditLogEntries::on_row_count(const ResultRow &row) {
  no_of_entries_ = atoi(row[0]);
}

void QueryAuditLogEntries::on_row_entries(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  auto &entry = entries.back();

  mysql_row.unserialize(&entry.id);
  mysql_row.unserialize(&entry.op);
  mysql_row.unserialize(&entry.table);
  mysql_row.unserialize_with_converter(&entry.old_table_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.new_table_id,
                                       entry::UniversalId::from_raw);

  if (max_id_ < entry.id) {
    max_id_ = entry.id;
  }
}

}  // namespace database
}  // namespace mrs
