/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_

#include <set>

#include "mrs/database/entry/universal_id.h"
#include "mrs/database/query_entries_audit_log.h"
#include "mrs/database/query_entries_auth_app.h"

namespace mrs {
namespace database {

template <typename QueryForAuthApps = v2::QueryEntriesAuthApp, int version = 2>
class QueryChangesAuthApp : public QueryForAuthApps {
 public:
  using Parent = QueryForAuthApps;
  using MySQLSession = mysqlrouter::MySQLSession;
  using Entries = typename Parent::Entries;

 public:
  QueryChangesAuthApp(const uint64_t last_audit_log_id) {
    Parent::audit_log_id_ = last_audit_log_id;
  }

  void query_entries(MySQLSession *session) override {
    QueryAuditLogEntries audit_entries;
    Entries local_entries;
    uint64_t max_audit_log_id = Parent::audit_log_id_;

    entries_fetched.clear();

    std::vector<std::string> allowed_changes{"auth_app", "auth_vendor",
                                             "service_has_auth_app"};

    if (version == 3) allowed_changes.push_back("service_has_auth_app");

    audit_entries.query_entries(session, allowed_changes,
                                Parent::audit_log_id_);

    for (const auto &audit_entry : audit_entries.entries) {
      auto table = audit_entry.table;
      if (audit_entry.old_table_id.has_value())
        query_auth_entries(session, &local_entries, table,
                           audit_entry.old_table_id.value());

      if (audit_entry.new_table_id.has_value())
        query_auth_entries(session, &local_entries, table,
                           audit_entry.new_table_id.value());

      if (max_audit_log_id < audit_entry.id) max_audit_log_id = audit_entry.id;
    }

    Parent::entries_.swap(local_entries);

    Parent::audit_log_id_ = max_audit_log_id;
  }

 private:
  void query_auth_entries(MySQLSession *session, Entries *out,
                          const std::string &table_name,
                          const entry::UniversalId &id) {
    Parent::entries_.clear();

    Parent::query(session, build_query(table_name, id));

    for (const auto &entry : Parent::entries_) {
      if (entries_fetched.count(entry.id)) continue;

      out->push_back(entry);
      entries_fetched.insert(entry.id);
    }

    if (Parent::entries_.empty() && table_name == "auth_app") {
      entry::AuthApp pe;
      pe.id = id;
      pe.deleted = true;
      entries_fetched.insert(id);
      out->push_back(pe);
    }
  }

  std::string build_query(std::string table_name,
                          const entry::UniversalId &id) {
    if ("service_has_auth_app" == table_name) {
      mysqlrouter::sqlstring where{
          " WHERE subtable.auth_app_id in (SELECT shaa.`auth_app_id`  FROM "
          "`mysql_rest_service_metadata`.`service_has_auth_app` as shaa "
          " WHERE `shaa`.`service_id`=? ) "};
      where << id;
      return Parent::query_.str() + where.str();
    }

    mysqlrouter::sqlstring where{" WHERE !=? "};
    where << (table_name + "_id") << id;

    return Parent::query_.str() + where.str();
  }

  std::set<entry::UniversalId> entries_fetched;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_
