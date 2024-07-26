/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "mrs/database/query_entries_db_schema.h"

#include <string>

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {

QueryEntriesDbSchema::QueryEntriesDbSchema() {
  query_ =
      "SELECT * FROM (SELECT"
      "  s.id, s.service_id, s.name, s.request_path, s.requires_auth,"
      "  s.enabled, s.items_per_page, s.comments, s.options"
      " FROM mysql_rest_service_metadata.`db_schema` as s ) as parent ";
}

uint64_t QueryEntriesDbSchema::get_last_update() { return audit_log_id_; }

void QueryEntriesDbSchema::query_entries(MySQLSession *session) {
  entries.clear();

  QueryAuditLogMaxId query_audit_id;
  MySQLSession::Transaction transaction(session);

  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);

  transaction.commit();

  audit_log_id_ = audit_log_id;
}

void QueryEntriesDbSchema::on_row(const ResultRow &row) {
  // const uint64_t k_on_page_default = 25;
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  DbSchema &entry = entries.back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.service_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize(&entry.request_path);
  mysql_row.unserialize(&entry.requires_auth);
  mysql_row.unserialize(&entry.enabled);
  mysql_row.unserialize(&entry.requires_auth);
  mysql_row.unserialize(&entry.items_per_page);
  mysql_row.unserialize(&entry.comment);
  mysql_row.unserialize(&entry.options);

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs
