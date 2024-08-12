/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CONNECTION_POOL_INCLUDED
#define MYSQLROUTER_CONNECTION_POOL_INCLUDED

#include "mysqlrouter/connection_pool_export.h"

#include <chrono>
#include <cstdint>  // uint32_t

#include <algorithm>  // find_if
#include <concepts>
#include <list>
#include <optional>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_types.h"  // Ssl
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/classic_protocol_state.h"
#include "mysqlrouter/connection_base.h"

// default max idle server connections set on bootstrap
static constexpr uint32_t kDefaultMaxIdleServerConnectionsBootstrap{64};

/**
 * pooled connection.
 */
class CONNECTION_POOL_EXPORT PooledConnectionBase {
 public:
  /**
   * set a remove-callback.
   *
   * used when the pooled connection wants to remove itself from the
   * connection-pool.
   */
  void remover(std::function<void()> remover) { remover_ = std::move(remover); }

  /**
   * calls remove-callback.
   */
  void remove_me();

  void reset();

 private:
  std::function<void()> remover_;
};

/**
 * pooled connection.
 */
template <class T>
class PooledConnection : public PooledConnectionBase {
 public:
  using Ssl = mysql_harness::Ssl;
  using connection_type = T;

  PooledConnection(connection_type conn)
      : conn_{std::move(conn)}, idle_timer_(conn_.connection()->io_ctx()) {}

  /**
   * access to conn_.
   *
   * allows others to move the connection structs out.
   */
  connection_type &connection() { return conn_; }

  const connection_type &connection() const { return conn_; }

  /**
   * prepares for reusing the connection.
   */
  void reset() {
    PooledConnectionBase::reset();

    (void)idle_timer_.cancel();
    (void)conn_.cancel();
  }

  friend class ConnectionPool;

 private:
  /**
   * wait for idle timeout.
   */
  void async_idle(std::chrono::milliseconds idle_timeout) {
    auto &tmr = idle_timer_;

    tmr.expires_after(idle_timeout);

    // if the idle_timer fires, close the connection and remove it from the
    // pool.
    tmr.async_wait([this](std::error_code ec) {
      if (ec) return;  // cancelled ...

      // timed out.
      //
      // cancel the async_recv() and remove the connection.
      (void)conn_.cancel();

      this->remove_me();
    });

    async_recv_message();
  }

  /**
   * wait for server message and shutdown.
   */
  void async_recv_message() {
    // for classic we may receive a ERROR for shutdown. Ignore
    // it and close the connection. for xprotocol we may
    // receive a NOTICE for shutdown. Ignore it and close the
    // connection.

    conn_.async_recv([this](std::error_code ec, size_t /* recved */) {
      if (ec) {
        if (ec == make_error_condition(net::stream_errc::eof)) {
          // cancel the timer and let that close the connection.
          idle_timer_.cancel();

          (void)conn_.close();

          this->remove_me();
        }
        return;
      }

      // discard what has been received.
      conn_.channel().recv_buffer().clear();

      // wait for the next bytes or connection-close.
      async_recv_message();
    });
  }

  connection_type conn_;

  net::steady_timer idle_timer_;
};

/**
 * connection pool of mysql connections.
 *
 * It can contain connections:
 *
 * - classic protocol
 * - to any tcp endpoint.
 *
 * It has:
 *
 * - a pool, which contains server-side connections without a client-connection
 * - a stash, which contains server-side connections with a client-connection
 *
 */
class CONNECTION_POOL_EXPORT ConnectionPool {
 public:
  using ServerSideConnection =
      TlsSwitchableConnection<ServerSideClassicProtocolState>;

  using connection_type = PooledConnection<ServerSideConnection>;

  using ConnectionIdentifier = void *;

  class ConnectionCloser {
   public:
    ConnectionCloser(ConnectionPool::ServerSideConnection conn)
        : conn_(std::move(conn)) {}

    void async_close();

    void async_send_quit();

    void await_quit_response(std::error_code ec, size_t transferred);

    ConnectionPool::ServerSideConnection &connection() { return conn_; }

    void before_close(
        std::function<void(const ConnectionPool::ServerSideConnection &)> cb) {
      before_close_ = std::move(cb);
    }

   private:
    ConnectionPool::ServerSideConnection conn_;

    std::function<void(const ConnectionPool::ServerSideConnection &)>
        before_close_;
  };

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

  /**
   * add a connection to the pool.
   *
   * if the pool is full, the connection will be close.
   */
  void add(ServerSideConnection conn);

  /**
   * add connection to the pool if the poll isn't full.
   */
  std::optional<ServerSideConnection> add_if_not_full(
      ServerSideConnection conn);

  /**
   * get a connection from the pool that matches a predicate.
   *
   * @returns a connection if one exists.
   */
  std::optional<ServerSideConnection> pop_if(
      const std::string &ep,
      std::predicate<const ServerSideConnection &> auto pred) {
    return pool_(
        [this, ep, pred](auto &pool) -> std::optional<ServerSideConnection> {
          auto key_range = pool.equal_range(ep);
          if (key_range.first == key_range.second) return std::nullopt;

          auto kv_it = std::find_if(
              key_range.first, key_range.second,
              [pred](const auto &kv) { return pred(kv.second.connection()); });
          if (kv_it == key_range.second) return std::nullopt;

          // found.

          auto pooled_conn = std::move(kv_it->second);

          pool.erase(kv_it);

          pooled_conn.reset();

          ++reused_;

          return std::move(pooled_conn.connection());
        });
  }

  void async_close_connection(ConnectionPool::ServerSideConnection conn);

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
   * add a server-side connection to the stash.
   *
   * @param conn server-side connection to be stashed.
   * @param from opaque connection-identifier
   * @param delay allow sharing with other connection after 'delay'
   * milliseconds.
   */
  void stash(ServerSideConnection conn, ConnectionIdentifier from,
             std::chrono::milliseconds delay);

  // discard all stashed connection and move them to the pool.
  void discard_all_stashed(ConnectionIdentifier from);

  /**
   * connection on the stash.
   */
  struct Stashed {
    // constructor for the container's .emplace()
    Stashed(PooledConnection<ServerSideConnection> pc, ConnectionIdentifier ci,
            std::chrono::steady_clock::time_point tp)
        : pooled_conn(std::move(pc)), conn_id(ci), after(tp) {}

    PooledConnection<ServerSideConnection> pooled_conn;  //!< pooled connection.
    ConnectionIdentifier conn_id;  //!< opaque connection identifier
    std::chrono::steady_clock::time_point after;  //!< stealable after ...
  };

  std::optional<ServerSideConnection> unstash_if(
      const std::string &ep,
      std::function<bool(const ServerSideConnection &)> pred,
      bool ignore_sharing_delay = false);

  std::optional<ServerSideConnection> unstash_mine(
      const std::string &ep, ConnectionIdentifier conn_id);

  /**
   * number of server-side connections on the stash.
   */
  [[nodiscard]] size_t current_stashed_connections() const;

  /**
   * total number of reused connections.
   */
  [[nodiscard]] uint64_t reused_connections() const { return reused_; }

 protected:
  using pool_type =
      std::unordered_multimap<std::string,
                              PooledConnection<ServerSideConnection>>;
  using stash_type = std::unordered_multimap<std::string, Stashed>;

  void erase(pool_type::iterator it);

  const uint32_t max_pooled_connections_;
  const std::chrono::milliseconds idle_timeout_;

  Monitor<pool_type> pool_{{}};

  Monitor<std::list<ConnectionCloser>> for_close_{{}};

  // a stash of sharable connections.
  //
  // they are associated to a connection.
  Monitor<stash_type> stash_{{}};

  void erase_from_stash(stash_type::iterator it);

  uint64_t reused_{};
};

#endif
