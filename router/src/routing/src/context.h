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

#ifndef ROUTING_CONTEXT_INCLUDED
#define ROUTING_CONTEXT_INCLUDED

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "mysql/harness/filesystem.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "tcp_address.h"
#include "utils.h"

class BaseProtocol;
namespace routing {
class RoutingSockOpsInterface;
}
namespace mysql_harness {
class SocketOperationsBase;
}

/**
 * @brief MySQLRoutingContext holds data used by MySQLRouting (1 per plugin
 * instances) and MySQLRoutingConnection instances (many instances). It is
 * created and owned by MySQLRouting while MySQLRoutingConnection objects hold
 * reference to it.
 */
class MySQLRoutingContext {
 public:
  MySQLRoutingContext(BaseProtocol *protocol,
                      mysql_harness::SocketOperationsBase *socket_operations,
                      const std::string &name, unsigned int net_buffer_length,
                      std::chrono::milliseconds destination_connect_timeout,
                      std::chrono::milliseconds client_connect_timeout,
                      const mysql_harness::TCPAddress &bind_address,
                      const mysql_harness::Path &bind_named_socket,
                      unsigned long long max_connect_errors,
                      size_t thread_stack_size);

  /** @brief Checks and if needed, blocks a host from using this routing
   *
   * Blocks a host from using this routing adding its IP address to the
   * list of blocked hosts when the maximum client errors has been
   * reached. Each call of this function will increment the number of
   * times it was called with the client IP address.
   *
   * When a client host is actually blocked, true will be returned,
   * otherwise false.
   *
   * @param client_ip_array IP address as array[16] of uint8_t
   * @param client_ip_str IP address as string (for logging purposes)
   * @param server Server file descriptor to wish to send
   *               fake handshake reply (default is not to send anything)
   * @return bool
   */
  bool block_client_host(const ClientIpArray &client_ip_array,
                         const std::string &client_ip_str, int server = -1);

  /** @brief Returns list of blocked client hosts
   *
   * Returns list of the blocked client hosts.
   */
  const std::vector<ClientIpArray> get_blocked_client_hosts() const;

  void increase_active_thread_counter();
  void decrease_active_thread_counter();
  void increase_info_active_routes();
  void decrease_info_active_routes();
  void increase_info_handled_routes();

  mysql_harness::SocketOperationsBase *get_socket_operations() {
    return socket_operations_;
  }

  BaseProtocol &get_protocol() { return *protocol_; }

  const std::string &get_name() const { return name_; }

  unsigned int get_net_buffer_length() const { return net_buffer_length_; }

  std::chrono::milliseconds get_destination_connect_timeout() const {
    return destination_connect_timeout_;
  }

  std::chrono::milliseconds get_client_connect_timeout() const {
    return client_connect_timeout_;
  }

  const mysql_harness::TCPAddress &get_bind_address() const {
    return bind_address_;
  }

  const mysql_harness::Path &get_bind_named_socket() const {
    return bind_named_socket_;
  }

  size_t get_thread_stack_size() const { return thread_stack_size_; }

 private:
  /** @brief object to handle protocol specific stuff */
  std::unique_ptr<BaseProtocol> protocol_;

  /** @brief object handling the operations on network sockets */
  mysql_harness::SocketOperationsBase *socket_operations_;

  /** @brief Descriptive name of the connection routing */
  const std::string name_;

  /** @brief Size of buffer to store receiving packets */
  unsigned int net_buffer_length_;

  /** @brief Timeout connecting to destination
   *
   * This timeout is used when trying to connect with a destination
   * server. When the timeout is reached, another server will be
   * tried. It is good to leave this time out 1 second or higher
   * if using an unstable network.
   */
  std::chrono::milliseconds destination_connect_timeout_;

  /** @brief Timeout waiting for handshake response from client */
  std::chrono::milliseconds client_connect_timeout_;

  /** @brief IP address and TCP port for setting up TCP service */
  const mysql_harness::TCPAddress bind_address_;

  /** @brief Path to named socket for setting up named socket service */
  const mysql_harness::Path bind_named_socket_;

  /** @brief memory in kilobytes allocated for thread's stack */
  size_t thread_stack_size_ = mysql_harness::kDefaultStackSizeInKiloBytes;

  mutable std::mutex mutex_conn_errors_;

 public:
  /** @brief Connection error counters for IPv4 or IPv6 hosts */
  std::map<ClientIpArray, size_t> conn_error_counters_;

  /** @brief Max connect errors blocking hosts when handshake not completed */
  unsigned long long max_connect_errors_;

  /** number of active client threads. */
  uint64_t active_client_threads_{0};
  std::condition_variable active_client_threads_cond_;
  std::mutex active_client_threads_cond_m_;

  /** @brief Number of active routes */
  std::atomic<uint16_t> info_active_routes_{0};
  /** @brief Number of handled routes, not used at the moment */
  std::atomic<uint64_t> info_handled_routes_{0};
};
#endif /* ROUTING_CONTEXT_INCLUDED */
