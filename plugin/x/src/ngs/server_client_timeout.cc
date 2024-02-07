/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/ngs/server_client_timeout.h"

#include "plugin/x/src/ngs/log.h"

namespace ngs {

Server_client_timeout::Server_client_timeout(
    const xpl::chrono::Time_point &release_all_before_time)
    : m_release_all_before_time(release_all_before_time) {}

void Server_client_timeout::validate_client_state(
    std::shared_ptr<xpl::iface::Client> client) {
  const xpl::chrono::Time_point client_accept_time = client->get_accept_time();
  const xpl::iface::Client::Client::State state = client->get_state();

  if (xpl::iface::Client::State::k_invalid == state ||
      xpl::iface::Client::State::k_accepted == state ||
      xpl::iface::Client::State::k_authenticating_first == state) {
    if (client_accept_time <= m_release_all_before_time) {
      log_debug("%s: release triggered by timeout in state:%i",
                client->client_id(), static_cast<int>(state));
      client->on_auth_timeout();
      return;
    }

    if (!xpl::chrono::is_valid(m_oldest_client_accept_time) ||
        m_oldest_client_accept_time > client_accept_time) {
      m_oldest_client_accept_time = client_accept_time;
    }
  }
}

xpl::chrono::Time_point Server_client_timeout::get_oldest_client_accept_time() {
  return m_oldest_client_accept_time;
}

}  // namespace ngs
