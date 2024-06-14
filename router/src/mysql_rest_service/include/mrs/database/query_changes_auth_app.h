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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_

#include <set>

#include "mrs/database/entry/universal_id.h"
#include "mrs/database/query_entries_audit_log.h"
#include "mrs/database/query_entries_auth_app.h"

namespace mrs {
namespace database {

template <typename QueryForAuthApps = v2::QueryEntriesAuthApp>
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
    MySQLSession::Transaction transaction(session);

    audit_entries.query_entries(
        session, {"service", "url_host", "auth_app", "auth_vendor"},
        Parent::audit_log_id_);

    for (const auto &audit_entry : audit_entries.entries) {
      if (audit_entry.old_table_id.has_value())
        query_auth_entries(session, &local_entries, audit_entry.table,
                           audit_entry.old_table_id.value());

      if (audit_entry.new_table_id.has_value())
        query_auth_entries(session, &local_entries, audit_entry.table,
                           audit_entry.new_table_id.value());

      if (max_audit_log_id < audit_entry.id) max_audit_log_id = audit_entry.id;
    }

    Parent::entries_.swap(local_entries);

    transaction.commit();

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

  std::string build_query(const std::string &table_name,
                          const entry::UniversalId &id) {
    mysqlrouter::sqlstring where{" WHERE !=? "};
    where << (table_name + "_id") << id;

    return Parent::query_.str() + where.str();
  }

  std::set<entry::UniversalId> entries_fetched;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_AUTHENTICATION_H_
