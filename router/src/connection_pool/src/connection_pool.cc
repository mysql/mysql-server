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

#include "mysqlrouter/connection_pool.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <chrono>
#include <tuple>
#include <utility>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/socket.h"

#include "mysqlrouter/classic_protocol_codec.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"

static void connection_quit(ConnectionBase *conn, SSL *ssl) {
  // try a best effort approach to send a COM_QUIT to the server before
  // closing.
  namespace cl = classic_protocol;
  using Msg = cl::message::client::Quit;
  using Frm = cl::frame::Frame<Msg>;

  constexpr Frm frm{0 /* seq-id */, {}};
  std::array<std::byte, cl::Codec<Frm>(frm, {}).size()> quit_msg{};
  auto enc_res = cl::Codec<Frm>(frm, {}).encode(net::buffer(quit_msg));
  if (!enc_res) {
    // ignore
  }

  auto fd = conn->native_handle();

  if (ssl != nullptr) {
    // encrypt the COM_QUIT message and append the TLS shutdown-alert.
    (void)SSL_write(ssl, quit_msg.data(), quit_msg.size());
    (void)SSL_shutdown(ssl);

    auto *bio = SSL_get_wbio(ssl);

    std::vector<uint8_t> wbuf(BIO_pending(bio));
    const auto read_res = BIO_read(bio, wbuf.data(), wbuf.size());

    assert(read_res > 0);

    if (read_res > 0) {
      (void)net::impl::socket::write(fd, wbuf.data(), wbuf.size());
    }

  } else {
    (void)net::impl::socket::write(fd, quit_msg.data(), quit_msg.size());
  }

  auto shutdown_res = net::impl::socket::shutdown(
      fd, static_cast<int>(net::socket_base::shutdown_both));
  if (shutdown_res) {
    using namespace std::chrono_literals;

    // if shutdown fails, the other side already closed the socket.
    //
    // otherwise, wait a bit to make sure that the quit-msg left the router
    // before closing the socket.

    // wait max 1ms.
    std::array<net::impl::poll::poll_fd, 1> fds{{{fd, POLLIN, 0}}};
    net::impl::poll::poll(fds.data(), fds.size(), 1ms);
  }

  // connection can be closed.
  (void)conn->close();  // will cancel the async_read()
}

void PooledConnectionBase::remove_me() {
  // call the remove_ callback once.
  if (remover_) std::exchange(remover_, nullptr)();
}

void PooledConnectionBase::reset() { remover_ = nullptr; }

void ConnectionPool::add(ConnectionPool::ServerSideConnection conn) {
  pool_([&](auto &pool) {
    if (pool.size() >= max_pooled_connections_) {
      connection_quit(conn.connection().get(), conn.channel().ssl());

      return;
    }

    conn.prepare_for_pool();

    auto it = pool.emplace(conn.endpoint(), std::move(conn));

    it->second.remover([this, it]() { erase(it); });
    it->second.async_idle(idle_timeout_);
  });
}

std::optional<ConnectionPool::ServerSideConnection>
ConnectionPool::add_if_not_full(ConnectionPool::ServerSideConnection conn) {
  return pool_(
      [&](auto &pool) -> std::optional<ConnectionPool::ServerSideConnection> {
        if (pool.size() >= max_pooled_connections_) {
          return std::move(conn);
        }

        conn.prepare_for_pool();

        auto it = pool.emplace(conn.endpoint(), std::move(conn));

        it->second.remover([this, it]() { erase(it); });
        it->second.async_idle(idle_timeout_);

        return std::nullopt;
      });
}

void ConnectionPool::stash(ServerSideConnection conn, ConnectionIdentifier from,
                           std::chrono::milliseconds delay) {
  conn.prepare_for_pool();

  return stash_(
      [this, ep = conn.endpoint(), &conn, from,
       after = std::chrono::steady_clock::now() + delay](auto &stash) {
        auto it = stash.emplace(
            std::piecewise_construct,                            //
            std::forward_as_tuple(ep),                           // key
            std::forward_as_tuple(std::move(conn), from, after)  // val
        );

        it->second.pooled_conn.remover([this, it]() { erase_from_stash(it); });
        it->second.pooled_conn.async_idle(idle_timeout_);
      });
}

void ConnectionPool::discard_all_stashed(ConnectionIdentifier from) {
  return stash_([this, from](auto &pool) mutable {
    // move all the connections from the stash to pool.
    for (auto cur = pool.begin(); cur != pool.end();) {
      Stashed &val = cur->second;

      if (val.conn_id != from) {
        ++cur;
        continue;
      }

      // stop all callbacks.
      val.pooled_conn.reset();

      // move the connection to the pool.
      add(std::move(val.pooled_conn.connection()));

      cur = pool.erase(cur);
    }
  });
}

std::optional<ConnectionPool::ServerSideConnection> ConnectionPool::unstash_if(
    const std::string &ep,
    std::function<bool(const ServerSideConnection &)> pred,
    bool ignore_sharing_delay) {
  return stash_([&](auto &stash) -> std::optional<ServerSideConnection> {
    auto key_range = stash.equal_range(ep);
    if (key_range.first == key_range.second) return std::nullopt;

    auto kv_it =
        ignore_sharing_delay
            ? std::find_if(key_range.first, key_range.second,
                           [pred](const auto &kv) {
                             return pred(kv.second.pooled_conn.connection());
                           })
            : std::find_if(key_range.first, key_range.second,
                           [pred, now = std::chrono::steady_clock::now()](
                               const auto &kv) {
                             return now >= kv.second.after &&
                                    pred(kv.second.pooled_conn.connection());
                           });
    if (kv_it == key_range.second) return std::nullopt;

    kv_it->second.pooled_conn.reset();

    ServerSideConnection server_conn =
        std::move(kv_it->second.pooled_conn.connection());

    stash.erase(kv_it);

    return server_conn;
  });
}

std::optional<ConnectionPool::ServerSideConnection>
ConnectionPool::unstash_mine(const std::string &ep,
                             ConnectionIdentifier conn_id) {
  return stash_([&](auto &stash) -> std::optional<ServerSideConnection> {
    auto key_range = stash.equal_range(ep);
    if (key_range.first == key_range.second) return std::nullopt;

    auto kv_it = std::find_if(
        key_range.first, key_range.second,
        [conn_id](const auto &kv) { return kv.second.conn_id == conn_id; });
    if (kv_it == key_range.second) return std::nullopt;

    kv_it->second.pooled_conn.reset();

    ServerSideConnection server_conn =
        std::move(kv_it->second.pooled_conn.connection());

    stash.erase(kv_it);

    return server_conn;
  });
}

uint32_t ConnectionPool::current_pooled_connections() const {
  return pool_([](const auto &pool) { return pool.size(); });
}

size_t ConnectionPool::current_stashed_connections() const {
  return stash_([](const auto &stash) { return stash.size(); });
}

void ConnectionPool::erase(pool_type::iterator it) {
  pool_([it](auto &pool) { pool.erase(it); });
}

void ConnectionPool::erase_from_stash(stash_type::iterator it) {
  stash_([it](auto &stash) { stash.erase(it); });
}
