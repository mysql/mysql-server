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

#include "mrs/database/query_changes_db_object.h"

#include <utility>
#include "helper/mysql_row.h"

#include "mrs/database/query_entries_audit_log.h"
#include "mrs/database/query_entry_fields.h"
#include "mrs/database/query_entry_group_row_security.h"
#include "mrs/database/query_entry_object.h"

namespace mrs {
namespace database {

const std::string kObjTableName = "object";
const std::string kObjRefTableName = "object_reference";
const std::string kObjFieldTableName = "object_field";

QueryChangesDbObject::QueryChangesDbObject(SupportedMrsMetadataVersion v,
                                           QueryFactory *query_factory,
                                           const uint64_t last_audit_id)
    : QueryEntriesDbObject(v, query_factory) {
  audit_log_id_ = last_audit_id;
  query_length_ = query_.str().length();
}

void QueryChangesDbObject::query_entries(MySQLSession *session) {
  path_entries_fetched.clear();

  MySQLSession::Transaction transaction(session);
  QueryAuditLogEntries audit_entries;
  std::vector<DbObjectCompatible> local_path_entries;
  uint64_t max_audit_log_id = audit_log_id_;
  audit_entries.query_entries(
      session,
      {"service", "db_schema", "db_object", "url_host", kObjTableName,
       kObjRefTableName, kObjFieldTableName},
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

  auto qgroup = query_factory_->create_query_group_row_security();
  auto qfields = query_factory_->create_query_fields();
  auto qobject = query_factory_->create_query_object();

  for (auto &e : local_path_entries) {
    qgroup->query_group_row_security(session, e.id);
    e.row_group_security = std::move(qgroup->get_result());
    qfields->query_parameters(session, e.id);
    auto &r = qfields->get_result();
    e.fields = std::move(r);

    qobject->query_entries(session, skip_starting_slash(e.db_schema),
                           skip_starting_slash(e.db_table), e.id);
    e.object_description = qobject->object;

    if (db_version_ == mrs::interface::kSupportedMrsMetadataVersion_2) {
      if (e.user_ownership_v2.has_value()) {
        auto &value = e.user_ownership_v2.value();
        auto field = e.object_description->get_column_field(value);

        if (field) {
          e.object_description->user_ownership_field.emplace();
          e.object_description->user_ownership_field->field = field;
          e.object_description->user_ownership_field->uid = field->id;
        }
      }
    }
  }

  entries_.swap(local_path_entries);

  transaction.commit();

  audit_log_id_ = max_audit_log_id;
}

void QueryChangesDbObject::query_path_entries(
    MySQLSession *session, std::vector<DbObjectCompatible> *out,
    const std::string &table_name, const entry::UniversalId &id) {
  entries_.clear();

  query(session, build_query(table_name, id));

  for (const auto &entry : entries_) {
    if (path_entries_fetched.count(entry.id)) continue;

    out->push_back(entry);
    path_entries_fetched.insert(entry.id);
  }

  if (entries_.empty() && table_name == "db_object") {
    DbObjectCompatible pe;
    pe.id = id;
    pe.deleted = true;
    path_entries_fetched.insert(id);
    out->push_back(pe);
  }
}

std::string QueryChangesDbObject::build_query(const std::string &table_name,
                                              const entry::UniversalId &id) {
  mysqlrouter::sqlstring query = query_;

  if (kObjTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (select db_object_id from "
        "mysql_rest_service_metadata.object as f where f.id=? GROUP BY "
        "db_object_id)";
    where << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  } else if (kObjRefTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (SELECT o.db_object_id FROM "
        "mysql_rest_service_metadata.object_field AS f JOIN "
        "mysql_rest_service_metadata.object AS o ON o.id=f.object_id WHERE "
        "(f.parent_reference_id=? or f.represents_reference_id=?) GROUP BY "
        "db_object_id)";
    where << id << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  } else if (kObjFieldTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (SELECT o.db_object_id FROM "
        "mysql_rest_service_metadata.object_field AS f JOIN "
        "mysql_rest_service_metadata.object AS o ON o.id=f.object_id WHERE "
        "f.id=? GROUP BY db_object_id)";
    where << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  }

  mysqlrouter::sqlstring where = " WHERE !=? ";
  where << (table_name + "_id") << id;
  query << mysqlrouter::sqlstring{""};

  return query.str() + where.str();
}

QueryChangesDbObjectLite::QueryChangesDbObjectLite(
    SupportedMrsMetadataVersion v, QueryFactory *query_factory,
    const uint64_t last_audit_id)
    : QueryEntriesDbObjectLite(v, query_factory) {
  audit_log_id_ = last_audit_id;
  query_length_ = query_.str().length();
}

void QueryChangesDbObjectLite::query_entries(MySQLSession *session) {
  path_entries_fetched.clear();

  MySQLSession::Transaction transaction(session);
  QueryAuditLogEntries audit_entries;
  std::vector<DbObjectCompatible> local_path_entries;
  uint64_t max_audit_log_id = audit_log_id_;
  audit_entries.query_entries(
      session,
      {"db_object", kObjTableName, kObjRefTableName, kObjFieldTableName},
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

  auto qgroup = query_factory_->create_query_group_row_security();
  auto qfields = query_factory_->create_query_fields();
  auto qobject = query_factory_->create_query_object();

  for (auto &e : local_path_entries) {
    qgroup->query_group_row_security(session, e.id);
    e.row_group_security = std::move(qgroup->get_result());
    qfields->query_parameters(session, e.id);
    auto &r = qfields->get_result();
    e.fields = std::move(r);

    qobject->query_entries(session, skip_starting_slash(e.schema_name),
                           skip_starting_slash(e.name), e.id);
    e.object_description = qobject->object;

    if (db_version_ == mrs::interface::kSupportedMrsMetadataVersion_2) {
      if (e.user_ownership_v2.has_value()) {
        auto &value = e.user_ownership_v2.value();
        auto field = e.object_description->get_column_field(value);

        if (field) {
          e.object_description->user_ownership_field.emplace();
          e.object_description->user_ownership_field->field = field;
          e.object_description->user_ownership_field->uid = field->id;
        }
      }
    }
  }

  entries_.swap(local_path_entries);

  transaction.commit();

  audit_log_id_ = max_audit_log_id;
}

void QueryChangesDbObjectLite::query_path_entries(
    MySQLSession *session, std::vector<DbObjectCompatible> *out,
    const std::string &table_name, const entry::UniversalId &id) {
  entries_.clear();

  query(session, build_query(table_name, id));

  for (const auto &entry : entries_) {
    if (path_entries_fetched.count(entry.id)) continue;

    out->push_back(entry);
    path_entries_fetched.insert(entry.id);
  }

  if (entries_.empty() && table_name == "db_object") {
    DbObjectCompatible pe;
    pe.id = id;
    pe.deleted = true;
    path_entries_fetched.insert(id);
    out->push_back(pe);
  }
}

std::string QueryChangesDbObjectLite::build_query(
    const std::string &table_name, const entry::UniversalId &id) {
  mysqlrouter::sqlstring query = query_;

  if (kObjTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (select db_object_id from "
        "mysql_rest_service_metadata.object as f where f.id=? GROUP BY "
        "db_object_id)";
    where << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  } else if (kObjRefTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (SELECT o.db_object_id FROM "
        "mysql_rest_service_metadata.object_field AS f JOIN "
        "mysql_rest_service_metadata.object AS o ON o.id=f.object_id WHERE "
        "(f.parent_reference_id=? or f.represents_reference_id=?) GROUP BY "
        "db_object_id)";
    where << id << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  } else if (kObjFieldTableName == table_name) {
    mysqlrouter::sqlstring where =
        " WHERE id in (SELECT o.db_object_id FROM "
        "mysql_rest_service_metadata.object_field AS f  JOIN "
        "mysql_rest_service_metadata.object AS o ON o.id=f.object_id WHERE "
        "f.id=? GROUP BY db_object_id)";
    where << id;
    query << mysqlrouter::sqlstring{""};
    return query.str() + where.str();
  }

  mysqlrouter::sqlstring where = " WHERE !=? ";
  where << (table_name + "_id") << id;
  query << mysqlrouter::sqlstring{""};

  return query.str() + where.str();
}

}  // namespace database
}  // namespace mrs
