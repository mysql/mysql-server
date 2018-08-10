/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "connection_container.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

void ConnectionContainer::add_connection(
    std::unique_ptr<MySQLRoutingConnection> connection) {
  connections_.put(connection.get(), std::move(connection));
}

void ConnectionContainer::disconnect(const AllowedNodes &nodes) {
  unsigned number_of_disconnected_connections = 0;

  auto mark_to_diconnect_if_not_allowed =
      [&nodes, &number_of_disconnected_connections](
          std::pair<MySQLRoutingConnection *const,
                    std::unique_ptr<MySQLRoutingConnection>> &connection) {
        const auto &server_address = connection.first->get_server_address();
        const std::string &client_address =
            connection.first->get_client_address();
        if (std::find(nodes.begin(), nodes.end(), server_address) ==
            nodes.end()) {
          log_info("Disconnecting client %s from server %s",
                   client_address.c_str(), server_address.str().c_str());
          connection.first->disconnect();
          ++number_of_disconnected_connections;
        }
      };

  connections_.for_each(mark_to_diconnect_if_not_allowed);
  if (number_of_disconnected_connections > 0)
    log_info("Disconnected %u connections", number_of_disconnected_connections);
}

void ConnectionContainer::disconnect_all() {
  auto mark_to_disconnect =
      [](std::pair<MySQLRoutingConnection *const,
                   std::unique_ptr<MySQLRoutingConnection>> &connection) {
        connection.first->disconnect();
      };

  connections_.for_each(mark_to_disconnect);
}

void ConnectionContainer::remove_connection(
    MySQLRoutingConnection *connection) {
  connections_.erase(connection);
}
