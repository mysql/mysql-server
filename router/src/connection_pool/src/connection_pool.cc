/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/connection_pool.h"

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/socket.h"

void PooledConnection::async_recv_message() {
  // for classic we may receive a ERROR for shutdown. Ignore
  // it and close the connection. for xprotocol we may
  // receive a NOTICE for shutdown. Ignore it and close the
  // connection.

  conn_->async_recv(recv_buf_, [this](std::error_code ec, size_t /* recved */) {
    if (ec) {
      if (ec == make_error_condition(net::stream_errc::eof)) {
        // cancel the timer and let that close the connection.
        idle_timer_.cancel();

        (void)conn_->close();

        this->remove_me();
      }
      return;
    }

    // discard what has been received.
    recv_buf_.clear();

    // wait for the next bytes or connection-close.
    async_recv_message();
  });
}

void PooledConnection::async_idle(std::chrono::milliseconds idle_timeout) {
  idle_timer_.expires_after(idle_timeout);

  // if the idle_timer fires, close the connection and remove it from the pool.
  idle_timer_.async_wait([this](std::error_code ec) {
    if (ec) {
      // either success or cancelled.
      return;
    }

    if (conn_) {
      // connection can be closed.
      (void)conn_->close();  // will cancel the async_read()

      this->remove_me();
    }
  });

  async_recv_message();
}

void PooledConnection::remove_me() {
  // call the remove_ callback once.
  if (remover_) std::exchange(remover_, nullptr)();
}

void PooledConnection::reset() {
  idle_timer_.cancel();
  remover_ = nullptr;

  // stop the async_wait_recv()
  (void)conn_->cancel();
}

void ConnectionPool::add(ConnectionPool::connection_type conn) {
  pool_([&](auto &pool) {
    if (pool.size() >= max_pooled_connections_) return;

    pool.push_back(std::move(conn));

    auto &last = pool.back();
    last.remover([this, it = std::prev(pool.end())]() { erase(it); });
    last.async_idle(idle_timeout_);
  });
}

std::optional<ConnectionPool::connection_type> ConnectionPool::add_if_not_full(
    ConnectionPool::connection_type conn) {
  return pool_(
      [&](auto &pool) -> std::optional<ConnectionPool::connection_type> {
        if (pool.size() >= max_pooled_connections_) return std::move(conn);

        pool.push_back(std::move(conn));

        auto &last = pool.back();
        last.remover([this, it = std::prev(pool.end())]() { erase(it); });
        last.async_idle(idle_timeout_);

        return std::nullopt;
      });
}

uint32_t ConnectionPool::current_pooled_connections() const {
  return pool_([](const auto &pool) { return pool.size(); });
}

void ConnectionPool::erase(container_type::iterator it) {
  pool_([it](auto &pool) { pool.erase(it); });
}
