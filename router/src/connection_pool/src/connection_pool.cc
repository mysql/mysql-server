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
#include <system_error>
#include <tuple>
#include <utility>

#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/socket.h"

#include "mysql/harness/tls_error.h"
#include "mysqlrouter/classic_protocol_codec.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"

void ConnectionPool::ConnectionCloser::async_close() { async_send_quit(); }

void ConnectionPool::ConnectionCloser::async_send_quit() {
  // try a best effort approach to send a COM_QUIT to the server before
  // closing.
  namespace cl = classic_protocol;
  using Msg = cl::message::client::Quit;
  using Frm = cl::frame::Frame<Msg>;

  constexpr Frm frm{0 /* seq-id */, {}};

  auto &snd_buf = conn_.channel().send_plain_buffer();

  auto frame_size = cl::Codec<Frm>(frm, {}).size();
  snd_buf.resize(frame_size);

  auto enc_res = cl::Codec<Frm>(frm, {}).encode(net::buffer(snd_buf));
  if (!enc_res) {
    // ignore
  }

  conn_.channel().flush_to_send_buf();

  if (conn_.channel().ssl() != nullptr) {
    conn_.channel().tls_shutdown();
  }

  conn_.async_send(
      [&](std::error_code ec, size_t transferred [[maybe_unused]]) {
        if (ec) {
          // something failed.

          before_close_(conn_);
          return;
        }

        conn_.async_recv([&](std::error_code ec, size_t transferred) {
          await_quit_response(ec, transferred);
        });
      });
}

void ConnectionPool::ConnectionCloser::await_quit_response(std::error_code ec,
                                                           size_t transferred
                                                           [[maybe_unused]]) {
  // wait for the server's response.
  //
  // Either it closes the socket or sends a TLS shutdown reply.

  if (ec) {
    if (ec == net::stream_errc::eof && conn_.channel().ssl() != nullptr) {
      // call the TLS shutdown a 2nd time to ensure that the session can be
      // reused.
      conn_.channel().tls_shutdown();
    }

    before_close_(conn_);
    return;
  }

  conn_.channel().recv_buffer().clear();

  // receive until the socket gets closed.
  conn_.async_recv([&](std::error_code ec, size_t transferred) {
    await_quit_response(ec, transferred);
  });
}

void PooledConnectionBase::remove_me() {
  // call the remove_ callback once.
  if (remover_) std::exchange(remover_, nullptr)();
}

void PooledConnectionBase::reset() { remover_ = nullptr; }

void ConnectionPool::add(ConnectionPool::ServerSideConnection conn) {
  auto is_full_res = add_if_not_full(std::move(conn));
  if (is_full_res) {
    // pool is full, move the connection to the "for-close" pool
    // where it a COM_QUIT will be sent and the connection get closed
    // asynchronously.
    async_close_connection(std::move(*is_full_res));
  }
}

void ConnectionPool::async_close_connection(
    ConnectionPool::ServerSideConnection conn) {
  for_close_([&](auto &pool) {
    auto &closer = pool.emplace_back(std::move(conn));

    closer.before_close([this](const auto &to_be_closed_conn) {
      for_close_([&to_be_closed_conn](auto &pool) {
        auto fd = to_be_closed_conn.native_handle();
        auto removed = std::erase_if(pool, [fd](auto &el) {
          return el.connection().native_handle() == fd;
        });

        if (removed != 1) {
          harness_assert_this_should_not_execute();
        }
      });
    });

    closer.async_close();
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

        it->second.remover([this, it]() {
          if (it->second.connection().is_open()) {
            // move it to the async-closer.
            async_close_connection(std::move(it->second.connection()));
          }

          erase(it);
        });
        it->second.async_idle(idle_timeout_);

        return std::nullopt;
      });
}

void ConnectionPool::stash(ServerSideConnection conn, ConnectionIdentifier from,
                           std::chrono::milliseconds delay) {
  conn.prepare_for_pool();

  return stash_([this, ep = conn.endpoint(), &conn, from,
                 after =
                     std::chrono::steady_clock::now() + delay](auto &stash) {
    auto it = stash.emplace(
        std::piecewise_construct,                            //
        std::forward_as_tuple(ep),                           // key
        std::forward_as_tuple(std::move(conn), from, after)  // val
    );

    it->second.pooled_conn.remover([this, it]() {
      if (it->second.pooled_conn.connection().is_open()) {
        // move it to the async-closer.
        async_close_connection(std::move(it->second.pooled_conn.connection()));
      }

      erase_from_stash(it);
    });
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
