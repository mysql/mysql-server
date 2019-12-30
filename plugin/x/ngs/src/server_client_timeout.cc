/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/ngs/include/ngs/server_client_timeout.h"

#include "plugin/x/ngs/include/ngs/log.h"

namespace ngs {

Server_client_timeout::Server_client_timeout(
    const xpl::chrono::Time_point &release_all_before_time)
    : m_release_all_before_time(release_all_before_time) {}

void Server_client_timeout::validate_client_state(
    std::shared_ptr<Client_interface> client) {
  const xpl::chrono::Time_point client_accept_time = client->get_accept_time();
  const auto state = client->get_state();

  if (Client_interface::State::k_accepted == state ||
      Client_interface::State::k_authenticating_first == state) {
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
