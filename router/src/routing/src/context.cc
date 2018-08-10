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

#include "context.h"
#include <cstring>

#include <memory>
#include "common.h"
#include "mysql/harness/logging/logging.h"
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

bool MySQLRoutingContext::block_client_host(
    const ClientIpArray &client_ip_array, const std::string &client_ip_str,
    int server) {
  bool blocked = false;
  {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    if (++conn_error_counters_[client_ip_array] >= max_connect_errors_) {
      log_warning("[%s] blocking client host %s", name_.c_str(),
                  client_ip_str.c_str());
      blocked = true;
    } else {
      log_info("[%s] %lu connection errors for %s (max %llu)", name_.c_str(),
               static_cast<unsigned long>(
                   conn_error_counters_[client_ip_array]),  // 32bit Linux
                                                            // requires cast
               client_ip_str.c_str(), max_connect_errors_);
    }
  }

  if (server >= 0) {
    protocol_->on_block_client_host(server, name_);
  }

  return blocked;
}

const std::vector<ClientIpArray> MySQLRoutingContext::get_blocked_client_hosts()
    const {
  std::lock_guard<std::mutex> lock(mutex_conn_errors_);

  std::vector<ClientIpArray> result;
  for (const auto &client_ip : conn_error_counters_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first);
    }
  }

  return result;
}

void MySQLRoutingContext::increase_active_thread_counter() {
  {
    std::lock_guard<std::mutex> lk(active_client_threads_cond_m_);
    active_client_threads_++;
  }
  active_client_threads_cond_.notify_all();
}

void MySQLRoutingContext::decrease_active_thread_counter() {
  {
    std::lock_guard<std::mutex> lk(active_client_threads_cond_m_);
    active_client_threads_--;
  }
  // notify the parent while we have the cond_mutex locked
  // otherwise the parent may destruct before we are finished.
  active_client_threads_cond_.notify_all();
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
