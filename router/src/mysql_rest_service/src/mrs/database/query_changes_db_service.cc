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

#include "mrs/database/query_changes_db_service.h"

#include "mrs/database/query_entries_audit_log.h"

namespace mrs {
namespace database {

QueryChangesDbService::QueryChangesDbService(SupportedMrsMetadataVersion v,
                                             const uint64_t last_audit_log_id)
    : QueryEntriesDbService(v) {
  audit_log_id_ = last_audit_log_id;
}

void QueryChangesDbService::query_entries(MySQLSession *session) {
  QueryAuditLogEntries audit_entries;
  VectorOfEntries local_entries;
  uint64_t max_audit_log_id = audit_log_id_;

  entries_fetched.clear();
  MySQLSession::Transaction transaction(session);

  audit_entries.query_entries(session, {"service"}, audit_log_id_);

  for (const auto &audit_entry : audit_entries.entries) {
    if (audit_entry.old_table_id.has_value())
      query_service_entries(session, &local_entries, audit_entry.table,
                            audit_entry.old_table_id.value());

    if (audit_entry.new_table_id.has_value())
      query_service_entries(session, &local_entries, audit_entry.table,
                            audit_entry.new_table_id.value());

    if (max_audit_log_id < audit_entry.id) max_audit_log_id = audit_entry.id;
  }

  entries.swap(local_entries);

  transaction.commit();

  audit_log_id_ = max_audit_log_id;
}

void QueryChangesDbService::query_service_entries(MySQLSession *session,
                                                  VectorOfEntries *out,
                                                  const std::string &table_name,
                                                  const entry::UniversalId id) {
  entries.clear();

  query(session, build_query(table_name, id));

  for (const auto &entry : entries) {
    if (entries_fetched.count(entry.id)) continue;

    out->push_back(entry);
    entries_fetched.insert(entry.id);
  }

  if (entries.empty() && table_name == "service") {
    entry::DbService se;
    se.id = id;
    se.deleted = true;
    entries_fetched.insert(id);
    out->push_back(se);
  }
}

std::string QueryChangesDbService::build_query(const std::string &table_name,
                                               const entry::UniversalId id) {
  mysqlrouter::sqlstring where{" WHERE !=?"};
  where << (table_name + "_id") << id;
  return query_.str() + where.str();
}

}  // namespace database
}  // namespace mrs
