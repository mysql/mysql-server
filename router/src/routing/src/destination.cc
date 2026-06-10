/*
  Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

#include "destination.h"

#include <mutex>      // lock_guard
#include <stdexcept>  // out_of_range

#include "mysql/harness/destination.h"
#include "mysql/harness/string_utils.h"  //ieq

AllowedNodesChangeCallbacksListIterator
DestinationNodesStateNotifier::register_allowed_nodes_change_callback(
    const AllowedNodesChangedCallback &clb) {
  std::lock_guard<std::mutex> lock(allowed_nodes_change_callbacks_mtx_);
  return allowed_nodes_change_callbacks_.insert(
      allowed_nodes_change_callbacks_.end(), clb);
}

void DestinationNodesStateNotifier::unregister_allowed_nodes_change_callback(
    const AllowedNodesChangeCallbacksListIterator &it) {
  std::lock_guard<std::mutex> lock(allowed_nodes_change_callbacks_mtx_);
  allowed_nodes_change_callbacks_.erase(it);
}

void DestinationNodesStateNotifier::register_start_router_socket_acceptor(
    const StartSocketAcceptorCallback &callback) {
  std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
  start_router_socket_acceptor_callback_ = callback;
}

void DestinationNodesStateNotifier::unregister_start_router_socket_acceptor() {
  std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
  start_router_socket_acceptor_callback_ = nullptr;
}

void DestinationNodesStateNotifier::register_stop_router_socket_acceptor(
    const StopSocketAcceptorCallback &callback) {
  std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
  stop_router_socket_acceptor_callback_ = callback;
}

void DestinationNodesStateNotifier::unregister_stop_router_socket_acceptor() {
  std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
  stop_router_socket_acceptor_callback_ = nullptr;
}

void DestinationNodesStateNotifier::register_md_refresh_callback(
    const MetadataRefreshCallback &callback) {
  std::lock_guard<std::mutex> lock(md_refresh_callback_mtx_);
  md_refresh_callback_ = callback;
}

void DestinationNodesStateNotifier::unregister_md_refresh_callback() {
  std::lock_guard<std::mutex> lock(md_refresh_callback_mtx_);
  md_refresh_callback_ = nullptr;
}

void DestinationNodesStateNotifier::register_query_quarantined_destinations(
    const QueryQuarantinedDestinationsCallback &callback) {
  std::lock_guard<std::mutex> lock(
      query_quarantined_destinations_callback_mtx_);
  query_quarantined_destinations_callback_ = callback;
}

void DestinationNodesStateNotifier::
    unregister_query_quarantined_destinations() {
  std::lock_guard<std::mutex> lock(
      query_quarantined_destinations_callback_mtx_);
  query_quarantined_destinations_callback_ = nullptr;
}

bool DestinationNodesStateNotifier::is_dynamic() { return false; }

std::string DestinationNodesStateNotifier::get_dynamic_plugin_name() {
  return {};
}

mysqlrouter::ServerMode Destination::server_mode() const {
  if (mysql_harness::ieq(server_info_.member_role, "PRIMARY"))
    return mysqlrouter::ServerMode::ReadWrite;
  else if (mysql_harness::ieq(server_info_.member_role, "SECONDARY") ||
           mysql_harness::ieq(server_info_.member_role, "READ_REPLICA"))
    return mysqlrouter::ServerMode::ReadOnly;

  return mysqlrouter::ServerMode::Unavailable;
}
