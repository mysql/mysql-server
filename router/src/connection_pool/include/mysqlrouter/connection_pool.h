/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CONNECTION_POOL_INCLUDED
#define MYSQLROUTER_CONNECTION_POOL_INCLUDED

#include "mysqlrouter/connection_pool_export.h"

#include <chrono>
#include <cstdint>  // uint32_t

#include <algorithm>  // find_if
#include <list>
#include <optional>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_types.h"  // Ssl
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/connection_base.h"

#include "../../routing/src/ssl_mode.h"  // TODO(jkneschk)

/**
 * pooled connection.
 */
class CONNECTION_POOL_EXPORT PooledConnection {
 public:
  using Ssl = mysql_harness::Ssl;

  PooledConnection(std::unique_ptr<ConnectionBase> conn)
      : conn_{std::move(conn)},
        ssl_{},
        is_authenticated_{false},
        endpoint_{conn_->endpoint()} {}

  PooledConnection(std::unique_ptr<ConnectionBase> conn, Ssl ssl)
      : conn_{std::move(conn)},
        ssl_{std::move(ssl)},
        is_authenticated_{true},
        endpoint_{conn_->endpoint()} {}

  /**
   * access to conn_.
   *
   * allows others to move the connection structs out.
   */
  std::unique_ptr<ConnectionBase> &connection() { return conn_; }

  const std::unique_ptr<ConnectionBase> &connection() const { return conn_; }

  /**
   * access to ssl_.
   *
   * allows others to move the Ssl structs out.
   */
  Ssl &ssl() { return ssl_; }

  const Ssl &ssl() const { return ssl_; }

  /**
   * set a remove-callback.
   *
   * used when the pooled connection wants to remove itself from the
   * connection-pool.
   */
  void remover(std::function<void()> remover) { remover_ = std::move(remover); }

  /**
   * prepares for reusing the connection.
   */
  void reset();

  friend class ConnectionPool;

  /**
   * a pooled connection may be authenticated or not.
   *
   * not authenticated:
   *
   * - connection expects a fresh handshake
   *
   * authenticated:
   *
   * - connection expects a change-user
   *
   */
  bool is_authenticated() const { return is_authenticated_; }

  std::string endpoint() const { return endpoint_; }

 private:
  /**
   * wait for idle timeout.
   */
  void async_idle(std::chrono::milliseconds idle_timeout);

  /**
   * wait for server message and shutdown.
   */
  void async_recv_message();

  /**
   * calls remove-callback.
   */
  void remove_me();

  std::unique_ptr<ConnectionBase> conn_;
  std::function<void()> remover_;

  Ssl ssl_;

  ConnectionBase::recv_buffer_type
      recv_buf_;  // recv-buf for async_recv_message()
  net::steady_timer idle_timer_{conn_->io_ctx()};  // timer for async_idle()

  bool is_authenticated_{false};

  std::string endpoint_;
};

class CONNECTION_POOL_EXPORT PooledClassicConnection : public PooledConnection {
 public:
  using caps_type = classic_protocol::capabilities::value_type;

  PooledClassicConnection(std::unique_ptr<ConnectionBase> conn)
      : PooledConnection{std::move(conn)} {}

  PooledClassicConnection(
      std::unique_ptr<ConnectionBase> conn, Ssl ssl, caps_type server_caps,
      caps_type client_caps,
      std::optional<classic_protocol::message::server::Greeting>
          server_greeting,
      SslMode ssl_mode, std::string username, std::string schema,
      std::string attributes)
      : PooledConnection{std::move(conn), std::move(ssl)},
        server_caps_{server_caps},
        client_caps_{client_caps},
        server_greeting_{std::move(server_greeting)},
        ssl_mode_{ssl_mode},
        username_{std::move(username)},
        schema_{std::move(schema)},
        attributes_{std::move(attributes)} {}

  [[nodiscard]] caps_type client_capabilities() const { return client_caps_; }

  [[nodiscard]] caps_type server_capabilities() const { return server_caps_; }

  [[nodiscard]] caps_type shared_capabilities() const {
    return server_caps_ & client_caps_;
  }

  std::optional<classic_protocol::message::server::Greeting> server_greeting()
      const {
    return server_greeting_;
  }

  SslMode ssl_mode() const { return ssl_mode_; }

  std::string username() const { return username_; }
  std::string schema() const { return schema_; }
  std::string attributes() const { return attributes_; }

 private:
  caps_type server_caps_{};
  caps_type client_caps_{};

  std::optional<classic_protocol::message::server::Greeting> server_greeting_;

  SslMode ssl_mode_;

  std::string username_;
  std::string schema_;
  std::string attributes_;
};

/**
 * connection pool of mysql connections.
 *
 * pool can contain connections:
 *
 * - of any protocol.
 * - to any tcp endpoint.
 *
 */
class CONNECTION_POOL_EXPORT ConnectionPool {
 public:
  using connection_type = PooledClassicConnection;

  ConnectionPool(uint32_t max_pooled_connections,
                 std::chrono::milliseconds idle_timeout)
      : max_pooled_connections_(max_pooled_connections),
        idle_timeout_(idle_timeout) {}

  // disable copy
  ConnectionPool(const ConnectionPool &) = delete;
  ConnectionPool &operator=(const ConnectionPool &) = delete;

  // disable move
  ConnectionPool(ConnectionPool &&) = delete;
  ConnectionPool &operator=(ConnectionPool &&) = delete;

  ~ConnectionPool() = default;

  void add(connection_type conn);

  /**
   * add connection to the pool if the poll isn't full.
   */
  std::optional<connection_type> add_if_not_full(connection_type conn);

  /**
   * get a connection from the pool that matches a predicate.
   *
   * @returns a connection if one exists.
   */
  template <class UnaryPredicate>
  std::optional<connection_type> pop_if(UnaryPredicate pred) {
    return pool_(
        [this,
         &pred](auto &pool) -> std::optional<ConnectionPool::connection_type> {
          auto it = std::find_if(pool.begin(), pool.end(), pred);

          if (it == pool.end()) return {};

          auto pooled_conn = std::move(*it);

          pool.erase(it);

          pooled_conn.reset();

          ++reused_;

          return pooled_conn;
        });
  }

  /**
   * number of currently pooled connections.
   */
  [[nodiscard]] uint32_t current_pooled_connections() const;

  [[nodiscard]] uint32_t max_pooled_connections() const {
    return max_pooled_connections_;
  }

  [[nodiscard]] std::chrono::milliseconds idle_timeout() const {
    return idle_timeout_;
  }

  /**
   * total number of reused connections.
   */
  [[nodiscard]] uint64_t reused_connections() const { return reused_; }

 protected:
  using container_type = std::list<connection_type>;

  void erase(container_type::iterator it);

  const uint32_t max_pooled_connections_;
  const std::chrono::milliseconds idle_timeout_;

  Monitor<std::list<connection_type>> pool_{{}};

  uint64_t reused_{};
};

#endif
