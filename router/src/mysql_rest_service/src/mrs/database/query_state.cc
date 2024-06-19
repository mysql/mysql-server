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

#include "mrs/database/query_state.h"

#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {

void QueryState::query_state(MySQLSession *session) {
  MySQLSession::Transaction transaction(session);
  changed_ = false;
  query_state_impl(session, &transaction);
}

uint64_t QueryState::get_last_update() { return audit_log_id_; }

void QueryState::on_row(const ResultRow &r) {
  if (r.size() < 2) return;
  auto state_new = atoi(r[0]) ? stateOn : stateOff;

  if (r[1])
    json_data_ = r[1];
  else
    json_data_.clear();

  if (state_ != state_new) {
    changed_ = true;
    state_ = state_new;
  }
}

bool QueryState::was_changed() const { return changed_; }

std::string QueryState::get_json_data() { return json_data_; }

State QueryState::get_state() { return state_; }

void QueryState::query_state_impl(MySQLSession *session,
                                  MySQLSession::Transaction *transaction) {
  QueryAuditLogMaxId query_audit_id;
  auto audit_log_id = query_audit_id.query_max_id(session);
  query_ =
      "SELECT service_enabled,data FROM mysql_rest_service_metadata.config;";
  execute(session);
  transaction->commit();
  audit_log_id_ = audit_log_id;
}

}  // namespace database
}  // namespace mrs
