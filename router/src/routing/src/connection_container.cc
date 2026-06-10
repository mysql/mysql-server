/*
  Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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

#include "connection_container.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/datatypes.h"

IMPORT_LOG_FUNCTIONS()

void ConnectionContainer::add_connection(
    std::shared_ptr<MySQLRoutingConnectionBase> connection) {
  connections_.put(connection.get(), std::move(connection));
}

#if 0
namespace {
std::string to_string(mysqlrouter::ServerMode mode) {
  switch (mode) {
    case mysqlrouter::ServerMode::ReadWrite:
      return "RW";
    case mysqlrouter::ServerMode::ReadOnly:
      return "RO";
    case mysqlrouter::ServerMode::Unavailable:
      return "n/a";
  }

  return "?";
}
}  // namespace
#endif

unsigned ConnectionContainer::disconnect(const AllowedNodes &nodes) {
  unsigned number_of_disconnected_connections = 0;

#if 0
  {
    std::ostringstream oss;
    oss << "allowed: ";
    bool is_first{true};
    for (const auto &allowed_node : nodes) {
      if (!is_first) {
        oss << ", ";
      } else {
        is_first = false;
      }

      oss << "(" << allowed_node.address.str() << ", "
          << to_string(allowed_node.mode) << ")";
    }
    std::cerr << oss.str() << "\n";
  }
#endif

  auto mark_to_disconnect_if_not_allowed =
      [&allowed_nodes = nodes,
       &number_of_disconnected_connections](auto &connection_element) {
        auto *conn = connection_element.first;

        auto conn_ro_dest_id = conn->read_only_destination_id();
        auto conn_rw_dest_id = conn->read_write_destination_id();
#if 0
        std::cerr << "conn: "
                  << "ro: " << conn_ro_dest_id << ", "
                  << "rw: " << conn_rw_dest_id << "\n";
#endif

        bool ro_allowed{!conn_ro_dest_id.has_value()};
        bool rw_allowed{!conn_rw_dest_id.has_value()};

        for (const auto &allowed_node : allowed_nodes) {
          auto allowed_dest_id = allowed_node.destination;

          if (allowed_dest_id == conn_ro_dest_id) ro_allowed = true;
          if (allowed_dest_id == conn_rw_dest_id &&
              allowed_node.mode == mysqlrouter::ServerMode::ReadWrite) {
            rw_allowed = true;
          }

          // both are allowed.
          if (ro_allowed && rw_allowed) return;
        }

        auto stats = conn->get_stats();

        log_info("Disconnecting client %s from server %s",
                 stats.client_address.c_str(), stats.server_address.c_str());
        conn->disconnect();

        ++number_of_disconnected_connections;
      };

  connections_.for_each(mark_to_disconnect_if_not_allowed);
#if 0
  std::cerr << "marked: " << number_of_disconnected_connections << "\n";
#endif

  return number_of_disconnected_connections;
}

MySQLRoutingConnectionBase *ConnectionContainer::get_connection(
    const std::string &client_endpoint) {
  MySQLRoutingConnectionBase *ret = nullptr;

  auto lookup = [&ret, &client_endpoint](auto &connection) {
    if (ret) return;
    const auto client_address = connection.first->get_stats().client_address;
    if (client_address == client_endpoint) {
      ret = connection.first;
    }
  };

  connections_.for_each(lookup);

  return ret;
}

void ConnectionContainer::disconnect_all() {
  connections_.for_each(
      [](const auto &connection) { connection.first->disconnect(); });
}

void ConnectionContainer::remove_connection(
    MySQLRoutingConnectionBase *connection) {
  std::unique_lock<std::mutex> lk(connection_removed_cond_m_);

  connections_.erase(connection);

  connection_removed_cond_.notify_all();
}

void ConnectionContainer::disconnect_on_routing_guidelines_update(
    const routing_guidelines::Routing_guidelines_engine::RouteChanges
        &update_details) {
  auto on_routing_guidelines_update = [&update_details](auto &connection) {
    if (!connection.second) return;

    connection.second->wait_until_completed();

    // Static route connection, skip it
    if (connection.second->get_routing_source().empty()) return;

    const auto affected_guidelines_routes = update_details.affected_routes;
    if (std::cend(affected_guidelines_routes) !=
        std::find(std::cbegin(affected_guidelines_routes),
                  std::cend(affected_guidelines_routes),
                  connection.second->get_routing_source())) {
      auto guidelines = connection.second->context().get_routing_guidelines();

      const auto &dest_classification_res =
          guidelines->classify(connection.second->get_server_info(),
                               connection.second->context().get_router_info());

      const auto &new_route_classifcation =
          guidelines->classify(connection.second->get_session_info(),
                               connection.second->context().get_router_info());

      if (!new_route_classifcation.errors.empty() ||
          !dest_classification_res.errors.empty()) {
        for (const auto &err_group :
             {new_route_classifcation.errors, dest_classification_res.errors}) {
          for (const auto &err : err_group) {
            log_debug("Routing guidelines classification error(s): %s",
                      err.c_str());
          }
        }
        // There were classification errors, do not drop the connection as this
        // should not happen.
        return;
      }

      const auto &allowed_destination_classes =
          dest_classification_res.class_names;

      if (new_route_classifcation.route_name.empty()) {
        // There is no route that could be used for this connection
        connection.first->disconnect();
        return;
      }

      for (const auto &route : new_route_classifcation.destination_groups) {
        for (const auto &destination : allowed_destination_classes) {
          if (std::cend(route.destination_classes) !=
              std::find(std::begin(route.destination_classes),
                        std::cend(route.destination_classes), destination)) {
            // Connection should use this new route (the old one is not
            // there anymore)
            connection.second->set_routing_source(
                new_route_classifcation.route_name);
            // There is a route destination allowed by the guidelines, lets quit
            return;
          }
        }
      }
      // Not allowed route destination found, drop the connection
      connection.first->disconnect();
    }
  };
  connections_.for_each(on_routing_guidelines_update);
}
