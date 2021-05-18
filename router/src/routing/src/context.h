/*
  Copyright (c) 2018, 2021, Oracle and/or its affiliates.

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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "destination_ssl_context.h"
#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/tls_server_context.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/routing.h"
#include "protocol/base_protocol.h"
#include "ssl_mode.h"
#include "tcp_address.h"

/**
 * @brief MySQLRoutingContext holds data used by MySQLRouting (1 per plugin
 * instances) and MySQLRoutingConnection instances (many instances). It is
 * created and owned by MySQLRouting while MySQLRoutingConnection objects hold
 * reference to it.
 */
class MySQLRoutingContext {
 public:
  MySQLRoutingContext(BaseProtocol::Type protocol, std::string name,
                      unsigned int net_buffer_length,
                      std::chrono::milliseconds destination_connect_timeout,
                      std::chrono::milliseconds client_connect_timeout,
                      mysql_harness::TCPAddress bind_address,
                      mysql_harness::Path bind_named_socket,
                      unsigned long long max_connect_errors,
                      size_t thread_stack_size, SslMode client_ssl_mode,
                      TlsServerContext *client_ssl_ctx, SslMode server_ssl_mode,
                      DestinationTlsContext *dest_tls_context)
      : protocol_(protocol),
        name_(std::move(name)),
        net_buffer_length_(net_buffer_length),
        destination_connect_timeout_(destination_connect_timeout),
        client_connect_timeout_(client_connect_timeout),
        bind_address_(std::move(bind_address)),
        bind_named_socket_(std::move(bind_named_socket)),
        thread_stack_size_(thread_stack_size),
        client_ssl_mode_{client_ssl_mode},
        client_ssl_ctx_{client_ssl_ctx},
        server_ssl_mode_{server_ssl_mode},
        destination_tls_context_{dest_tls_context},
        max_connect_errors_{max_connect_errors} {}

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
   * @param endpoint IP address as array[16] of uint8_t
   * @return bool
   */
  template <class Protocol>
  bool block_client_host(const typename Protocol::endpoint &endpoint);

  /** @brief Clears error counter (if needed) for particular host
   *
   * Resets connection error counter that may eventually lead to blocking host's
   * IP, if it exceeds a preset threshold.
   *
   * @sa block_client_host()
   *
   * @param endpoint IP address as array[16] of uint8_t
   */
  template <class Protocol>
  void clear_error_counter(const typename Protocol::endpoint &endpoint);

  template <class Protocol>
  bool is_blocked(const typename Protocol::endpoint &endpoint) const;

  /** @brief Returns list of blocked client hosts
   *
   * Returns list of the blocked client hosts.
   */
  std::vector<std::string> get_blocked_client_hosts() const;

  void increase_info_active_routes();
  void decrease_info_active_routes();
  void increase_info_handled_routes();

  uint16_t get_active_routes() { return info_active_routes_; }
  uint64_t get_handled_routes() { return info_handled_routes_; }
  uint64_t get_max_connect_errors() { return max_connect_errors_; }

  BaseProtocol::Type get_protocol() { return protocol_; }

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

  SslMode source_ssl_mode() const noexcept { return client_ssl_mode_; }
  SslMode dest_ssl_mode() const noexcept { return server_ssl_mode_; }

  /**
   * get the SSL context for the client side of the route.
   */
  TlsServerContext *source_ssl_ctx() const { return client_ssl_ctx_; }

  /**
   * get the SSL context for the server side of the route.
   *
   * @param dest_id id of the destination
   *
   * @returns a TlsClientContext for the destination.
   * @retval nullptr if creating tls-context failed.
   */
  TlsClientContext *dest_ssl_ctx(const std::string &dest_id) {
    if (destination_tls_context_ == nullptr) return nullptr;

    return destination_tls_context_->get(dest_id);
  }

 private:
  /** protocol type. */
  BaseProtocol::Type protocol_;

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

  SslMode client_ssl_mode_{SslMode::kPreferred};
  TlsServerContext *client_ssl_ctx_{};

  SslMode server_ssl_mode_{SslMode::kPreferred};
  DestinationTlsContext *destination_tls_context_{};

 public:
  /** @brief Connection error counters for IPv4 hosts */
  std::map<net::ip::address_v4, size_t> conn_error_counters_v4_;

  /** @brief Connection error counters for IPv4 hosts */
  std::map<net::ip::address_v6, size_t> conn_error_counters_v6_;

  /** @brief Max connect errors blocking hosts when handshake not completed */
  unsigned long long max_connect_errors_;

  /** @brief Number of active routes */
  std::atomic<uint16_t> info_active_routes_{0};
  /** @brief Number of handled routes, not used at the moment */
  std::atomic<uint64_t> info_handled_routes_{0};
};
#endif /* ROUTING_CONTEXT_INCLUDED */
