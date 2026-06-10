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

#include "mrs/database/query_state.h"

#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {

using DbState = QueryState::DbState;

QueryState::QueryState(const std::optional<uint64_t> &router_id)
    : router_id_{router_id} {}

void QueryState::query_state(MySQLSession *session) {
  changed_ = false;
  query_state_impl(session, nullptr);
}

void QueryState::on_row(const ResultRow &r) {
  DbState new_state;

  if (r.size() < 2) return;
  has_rows_ = true;
  new_state.service_enabled = atoi(r[0]) > 0;

  if (r[1]) new_state.data = r[1];

  if (state_ != new_state) {
    changed_ = true;
    state_ = new_state;
  }
}

bool QueryState::was_changed() const { return changed_; }

const DbState &QueryState::get_state() const { return state_; }

void QueryState::query_state_impl(MySQLSession *session,
                                  MySQLSession::Transaction *) {
  query_ =
      "SELECT service_enabled, data FROM mysql_rest_service_metadata.config;";
  has_rows_ = false;
  execute(session);
  if (!has_rows_) {
    throw NoRows("QueryState: the query returned no data");
  }
}

}  // namespace database
}  // namespace mrs
