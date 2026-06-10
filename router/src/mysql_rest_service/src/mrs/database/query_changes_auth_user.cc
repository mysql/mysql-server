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

#include "mrs/database/query_changes_auth_user.h"

#include "mrs/database/query_entries_audit_log.h"

namespace mrs {
namespace database {

QueryChangesAuthUser::QueryChangesAuthUser(const uint64_t last_audit_log_id) {
  audit_log_id_ = last_audit_log_id;
}

void QueryChangesAuthUser::query_changed_ids(MySQLSession *session) {
  QueryAuditLogEntries audit_entries;
  uint64_t max_audit_log_id = audit_log_id_;

  // The first run is done on the init, we only use it to determine te initial
  // audit_log_id for the future.

  entries_fetched.clear();

  audit_entries.query_entries(session, {"mrs_user"}, audit_log_id_);

  for (const auto &audit_entry : audit_entries.entries) {
    if (!first_run_ && audit_entry.old_table_id.has_value())
      entries_fetched.push_back(
          std::make_pair(audit_entry.old_table_id.value(), audit_entry.op));

    if (!first_run_ && audit_entry.new_table_id.has_value())
      entries_fetched.push_back(
          std::make_pair(audit_entry.new_table_id.value(), audit_entry.op));

    if (max_audit_log_id < audit_entry.id) max_audit_log_id = audit_entry.id;
  }

  audit_log_id_ = max_audit_log_id;
  if (first_run_) first_run_ = false;
}

}  // namespace database
}  // namespace mrs
