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

#include "mrs/database/query_changes_db_object.h"

#include "helper/mysql_row.h"

#include "mrs/database/query_entries_audit_log.h"
#include "mrs/database/query_entry_fields.h"
#include "mrs/database/query_entry_group_row_security.h"

namespace mrs {
namespace database {

const std::string kParameterTableName = "field";

QueryChangesDbObject::QueryChangesDbObject(const uint64_t last_audit_id) {
  audit_log_id_ = last_audit_id;
  query_length_ = query_.str().length();
}

static bool is_table_id_is_in_set(const std::string &table) {
  return table == kParameterTableName;
}

void QueryChangesDbObject::query_entries(MySQLSession *session) {
  path_entries_fetched.clear();
  query(session, "START TRANSACTION");

  QueryAuditLogEntries audit_entries;
  VectorOfPathEntries local_path_entries;
  uint64_t max_audit_log_id = audit_log_id_;
  audit_entries.query_entries(
      session,
      {"service", "db_schema", "db_object", "url_host", kParameterTableName},
      audit_log_id_);

  for (const auto &audit_entry : audit_entries.entries) {
    if (audit_entry.old_table_id.has_value())
      query_path_entries(session, &local_path_entries, audit_entry.table,
                         audit_entry.old_table_id.value());

    if (audit_entry.new_table_id.has_value())
      query_path_entries(session, &local_path_entries, audit_entry.table,
                         audit_entry.new_table_id.value());

    if (max_audit_log_id < audit_entry.id) max_audit_log_id = audit_entry.id;
  }

  QueryEntryGroupRowSecurity qg;
  QueryEntryFields qp;
  for (auto &e : local_path_entries) {
    qg.query_group_row_security(session, e.id);
    e.row_group_security = std::move(qg.get_result());
    qp.query_parameters(session, e.id);
    e.fields = std::move(qp.get_result());
  }

  entries.swap(local_path_entries);

  query(session, "COMMIT");

  audit_log_id_ = max_audit_log_id;
}

void QueryChangesDbObject::query_path_entries(MySQLSession *session,
                                              VectorOfPathEntries *out,
                                              const std::string &table_name,
                                              const entry::UniversalId &id) {
  entries.clear();

  query(session, build_query(table_name, id));

  for (const auto &entry : entries) {
    if (path_entries_fetched.count(entry.id)) continue;

    out->push_back(entry);
    path_entries_fetched.insert(entry.id);
  }

  if (entries.empty() && table_name == "db_object") {
    DbObject pe;
    pe.id = id;
    pe.deleted = true;
    path_entries_fetched.insert(id);
    out->push_back(pe);
  }
}

std::string QueryChangesDbObject::build_query(const std::string &table_name,
                                              const entry::UniversalId &id) {
  auto is_set = is_table_id_is_in_set(table_name);
  mysqlrouter::sqlstring query = query_;

  if (is_set) {
    mysqlrouter::sqlstring where = "WHERE FIND_IN_SET(?, !)";
    mysqlrouter::sqlstring additonal_fields =
        ",(SELECT GROUP_CONCAT(p.id) FROM "
        "mysql_rest_service_metadata.field as p WHERE "
        "p.db_object_id=o.id GROUP BY p.db_object_id) as field_id";
    where << id << (table_name + "_id");
    query << additonal_fields;
    return query.str() + where.str();
  }

  mysqlrouter::sqlstring where = " WHERE !=? ";
  where << (table_name + "_id") << id;
  query << mysqlrouter::sqlstring{""};

  return query.str() + where.str();
}

}  // namespace database
}  // namespace mrs
