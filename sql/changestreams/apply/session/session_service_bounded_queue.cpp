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

#include "sql/changestreams/apply/session/session_service_bounded_queue.h"

namespace mysql::csa {

Session_service_bounded_queue::Session_service_bounded_queue(
    Session_service_psi psi_params)
    : m_psi(psi_params) {}

Session_service_bounded_queue::~Session_service_bounded_queue() {}

bool Session_service_bounded_queue::deinit() {
  m_all_sessions.clear();
  assert(m_sessions.size() == m_session_number);
  return false;
}

bool Session_service_bounded_queue::init(std::size_t,
                                         Relay_log_info *channel_rli) {
  // Mysql requires to destroy THD by the thread that created it - it
  // checks thread id, therefore, we need to preallocate in init()
  // acquire_session is not able to create an additional session - assertion
  // will fail during deinit()
  m_channel_rli = channel_rli;
  for (std::size_t id = 0; id < m_session_number; ++id) {
    auto session = std::make_shared<Relay_context>(id, m_channel_rli);
    m_all_sessions.insert(std::make_pair(id, session));
    m_thd_ids.insert(session->get_thd_id());
    m_sessions.enqueue(std::move(session));
  }
  return false;
}

Relay_context_ptr Session_service_bounded_queue::acquire_session(Task_id) {
  auto stop_waiting = []() -> bool { return false; };
  auto [session, validity] = m_sessions.dequeue(stop_waiting);
  static_cast<void>(validity);
  return session;
}

void Session_service_bounded_queue::release_session(Relay_context_ptr session,
                                                    Session_service::Task_id) {
  m_sessions.enqueue(std::move(session));
}

}  // namespace mysql::csa
