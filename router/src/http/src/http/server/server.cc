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

#include "http/server/server.h"

#include <memory>
#include <utility>

#include "http/server/http_counters.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/io_thread.h"

IMPORT_LOG_FUNCTIONS()

namespace http {
namespace server {

std::atomic<uint64_t> http_connections_created{0};
std::atomic<uint64_t> http_connections_closed{0};
std::atomic<uint64_t> http_connections_reused{0};

namespace {

template <typename Connection>
void disconnect(Connection &c) {
  auto &socket = c->get_socket();
  auto &io_ctx = socket.get_executor().context();

  // queue the cancel in the connections io-ctx to make it thread-safe.
  net::dispatch(io_ctx, [c]() { c->get_socket().cancel(); });
}

}  // namespace

Server::Server(TlsServerContext *tls_context, IoThreads *threads,
               server::Bind *bind_raw, server::Bind *bind_ssl)
    : tls_context_{tls_context},
      threads_{threads},
      current_thread_{threads_->begin()},
      bind_raw_(bind_raw),
      bind_ssl_{bind_ssl} {}

void Server::set_allowed_methods(const base::method::Bitset &methods) {
  allowed_methods_ = methods;
}

void Server::set_request_handler(RequestHandlerInterface *handler) {
  handler_ = handler;
}

void Server::start() {
  sync_state_.exchange(State::kInitializing, State::kRunning, [this]() {
    // After successful changing the state, just start listening.
    start_accepting();
  });
}

void Server::start_accepting() {
  if (bind_raw_) {
    bind_raw_->start_accepting_loop([this](auto sock) {
      this->on_new_connection(socket_move_to_io_thread(std::move(sock)));
    });
  }
  if (bind_ssl_) {
    bind_ssl_->start_accepting_loop([this](auto sock) {
      this->on_new_ssl_connection(socket_move_to_io_thread(std::move(sock)));
    });
  }
}

void Server::stop() {
  if (bind_raw_) bind_raw_->stop_accepting_loop();
  if (bind_ssl_) bind_ssl_->stop_accepting_loop();

  disconnect_all();

  sync_state_.wait(State::kStopped);
}

size_t Server::disconnect_all() {
  std::lock_guard<std::mutex> lock(mutex_connection_);
  auto count = connections_.size() + connections_ssl_.size();
  for (auto &c : connections_ssl_) {
    disconnect(c);
  }

  for (auto &c : connections_) {
    disconnect(c);
  }

  sync_state_.exchange({State::kInitializing, State::kRunning},
                       count ? State::kStopping : State::kStopped);

  return count;
}

// === SSL connections handling ===

void Server::on_new_ssl_connection(socket socket) {
  auto connection = std::make_shared<ServerConnectionTls>(
      ConnectionTls::IO{tls_context_, std::move(socket)},
      &this->allowed_methods_, handler_, this);

  {
    std::lock_guard<std::mutex> lock(mutex_connection_);
    connections_ssl_.push_back(connection);
  }

  http_connections_created++;

  connection->start();
}

void Server::on_connection_close(ConnectionTls *connection) {
  std::lock_guard<std::mutex> lock(mutex_connection_);

  for (auto it = connections_ssl_.begin(); it != connections_ssl_.end(); ++it) {
    if ((*it).get() == connection) {
      connections_ssl_.erase(it);
      break;
    }
  }

  http_connections_closed++;
  if (connections_.empty() && connections_ssl_.empty())
    sync_state_.exchange(State::kStopping, State::kStopped);
}

void Server::on_connection_io_error(ConnectionTls *, const std::error_code &) {
  // We do not need to do anything here,
  // the http::base::Connection, after the error closes the connection
  // which results in 'on_connection_close' call.

  // In future this callback may be used to count errors.
}

// === RAW connections handling ===

void Server::on_new_connection(socket socket) {
  auto connection = std::make_shared<ServerConnectionRaw>(
      std::move(socket), &this->allowed_methods_, handler_, this);

  {
    std::lock_guard<std::mutex> lock(mutex_connection_);
    connections_.push_back(connection);
  }

  http_connections_created++;

  connection->start();
}

void Server::on_connection_close(ConnectionRaw *connection) {
  std::lock_guard<std::mutex> lock(mutex_connection_);

  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    if ((*it).get() == connection) {
      connections_.erase(it);
      break;
    }
  }
  http_connections_closed++;

  if (connections_.empty() && connections_ssl_.empty())
    sync_state_.exchange(State::kStopping, State::kStopped);
}

void Server::on_connection_io_error(ConnectionRaw *, const std::error_code &) {
  // We do not need to do anything here,
  // the http::base::Connection, after the error closes the connection
  // which results in 'on_connection_close' call.

  // In future this callback may be used to count errors.
}

Server::socket Server::socket_move_to_io_thread(socket s) {
  auto protocol = s.local_endpoint().value().protocol();
  auto fd = s.release().value();

  Server::socket result{return_next_thread()->context()};
  result.assign(protocol, fd);

  return result;
}

IoThread *Server::return_next_thread() {
  if (threads_->end() == ++current_thread_) {
    current_thread_ = threads_->begin();
  }

  return std::addressof(*current_thread_);
}

}  // namespace server
}  // namespace http
