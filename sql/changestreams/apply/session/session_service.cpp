// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa {

std::size_t Session_service::get_session_number() {
  return m_all_sessions.size();
}

std::optional<Session_legacy_stats> Session_service::get_session_stats(
    std::size_t session_id) {
  auto it = m_all_sessions.find(session_id);
  if (it == m_all_sessions.end()) {
    return {};
  }
  Session_legacy_stats result;
  Relay_log_info *rli_ptr = it->second->get_relay_log_info();
  if (rli_ptr == nullptr) {
    return {};
  }

  mysql_mutex_lock(&rli_ptr->data_lock);
  rli_ptr->get_gtid_monitoring_info()->copy_info_to(&(result.m_ongoing_trx),
                                                    &(result.m_applied_trx));

  mysql_mutex_lock(&rli_ptr->err_lock);
  result.m_error.copy_from(rli_ptr->last_error());
  mysql_mutex_unlock(&rli_ptr->err_lock);

  mysql_mutex_unlock(&rli_ptr->data_lock);
  return result;
}

bool Session_service::has_thd_id(unsigned int id) const {
  return m_thd_ids.contains(id);
}

void Session_service::clean_sessions() {
  for (const auto &session_entry : m_all_sessions) {
    session_entry.second->clean();
  }
}

void Session_service::enable_stop_error_suppression_for_clean_sessions() {
  for (const auto &session_entry : m_all_sessions) {
    session_entry.second->enable_stop_error_suppression_if_clean();
  }
}

void Session_service::awake_sessions(bool force_kill) {
  for (const auto &session_entry : m_all_sessions) {
    session_entry.second->awake(force_kill);
  }
}

}  // namespace mysql::csa
