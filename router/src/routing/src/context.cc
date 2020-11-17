/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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

#include "context.h"

#include <cstring>
#include <memory>
#include <mutex>

#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/local.h"
#include "mysqlrouter/routing.h"
#include "protocol/base_protocol.h"
#include "utils.h"

IMPORT_LOG_FUNCTIONS()

MySQLRoutingContext ::MySQLRoutingContext(
    BaseProtocol *protocol,
    mysql_harness::SocketOperationsBase *socket_operations,
    const std::string &name, unsigned int net_buffer_length,
    std::chrono::milliseconds destination_connect_timeout,
    std::chrono::milliseconds client_connect_timeout,
    const mysql_harness::TCPAddress &bind_address,
    const mysql_harness::Path &bind_named_socket,
    unsigned long long max_connect_errors, size_t thread_stack_size)
    : protocol_(protocol),
      socket_operations_(socket_operations),
      name_(name),
      net_buffer_length_(net_buffer_length),
      destination_connect_timeout_(destination_connect_timeout),
      client_connect_timeout_(client_connect_timeout),
      bind_address_(bind_address),
      bind_named_socket_(bind_named_socket),
      thread_stack_size_(thread_stack_size),
      max_connect_errors_(max_connect_errors) {}

#ifdef NET_TS_HAS_UNIX_SOCKET
template <>
bool MySQLRoutingContext::is_blocked<local::stream_protocol>(
    const local::stream_protocol::endpoint & /* endpoint */) const {
  return false;
}
template <>
bool MySQLRoutingContext::block_client_host<local::stream_protocol>(
    const local::stream_protocol::endpoint & /* endpoint */, int /* server */) {
  return false;
}

template <>
void MySQLRoutingContext::clear_error_counter<local::stream_protocol>(
    const local::stream_protocol::endpoint & /* endpoint */) {}
#endif

template <>
bool MySQLRoutingContext::block_client_host<net::ip::tcp>(
    const net::ip::tcp::endpoint &endpoint, int server) {
  bool blocked = false;
  {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    const size_t connection_errors =
        endpoint.address().is_v4()
            ? ++conn_error_counters_v4_[endpoint.address().to_v4()]
            : ++conn_error_counters_v6_[endpoint.address().to_v6()];

    if (connection_errors >= max_connect_errors_) {
      log_warning("[%s] blocking client host %s", name_.c_str(),
                  endpoint.address().to_string().c_str());
      blocked = true;
    } else {
      log_info("[%s] %zu connection errors for %s (max %llu)", name_.c_str(),
               connection_errors, endpoint.address().to_string().c_str(),
               max_connect_errors_);
    }
  }

  if (server >= 0) {
    protocol_->on_block_client_host(server, name_);
  }

  return blocked;
}

template <>
void MySQLRoutingContext::clear_error_counter<net::ip::tcp>(
    const net::ip::tcp::endpoint &endpoint) {
  if (endpoint.address().is_v4()) {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    auto it = conn_error_counters_v4_.find(endpoint.address().to_v4());
    if (it != conn_error_counters_v4_.end() && it->second > 0) {
      log_info(
          "[%s] resetting connection error counter for %s from %zu back to 0",
          name_.c_str(), endpoint.address().to_string().c_str(), it->second);
      it->second = 0;
    }
  } else {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    auto it = conn_error_counters_v6_.find(endpoint.address().to_v6());
    if (it != conn_error_counters_v6_.end() && it->second > 0) {
      log_info(
          "[%s] resetting connection error counter for %s from %zu back to 0",
          name_.c_str(), endpoint.address().to_string().c_str(), it->second);
      it->second = 0;
    }
  }
}

template <>
bool MySQLRoutingContext::is_blocked<net::ip::tcp>(
    const net::ip::tcp::endpoint &endpoint) const {
  size_t connect_errors{};
  if (endpoint.address().is_v4()) {
    std::lock_guard<std::mutex> lk(mutex_conn_errors_);

    const auto it = conn_error_counters_v4_.find(endpoint.address().to_v4());
    if (it != conn_error_counters_v4_.end()) {
      connect_errors = it->second;
    }
  } else {
    std::lock_guard<std::mutex> lk(mutex_conn_errors_);

    const auto it = conn_error_counters_v6_.find(endpoint.address().to_v6());
    if (it != conn_error_counters_v6_.end()) {
      connect_errors = it->second;
    }
  }

  return connect_errors >= max_connect_errors_;
}

std::vector<std::string> MySQLRoutingContext::get_blocked_client_hosts() const {
  std::lock_guard<std::mutex> lock(mutex_conn_errors_);

  std::vector<std::string> result;
  for (const auto &client_ip : conn_error_counters_v4_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first.to_string());
    }
  }
  for (const auto &client_ip : conn_error_counters_v6_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first.to_string());
    }
  }

  return result;
}

void MySQLRoutingContext::increase_info_active_routes() {
  ++info_active_routes_;
}

void MySQLRoutingContext::decrease_info_active_routes() {
  --info_active_routes_;
}

void MySQLRoutingContext::increase_info_handled_routes() {
  ++info_handled_routes_;
}
