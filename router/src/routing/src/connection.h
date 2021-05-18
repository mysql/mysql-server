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

#ifndef ROUTING_CONNECTION_INCLUDED
#define ROUTING_CONNECTION_INCLUDED

#include <atomic>
#include <chrono>
#include <functional>  // bind
#include <memory>
#include <sstream>

#include "context.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql_router_thread.h"
#include "protocol/base_protocol.h"

IMPORT_LOG_FUNCTIONS()

class MySQLRouting;
class MySQLRoutingContext;

namespace mysql_harness {
class PluginFuncEnv;
}

class MySQLRoutingConnectionBase {
 public:
  MySQLRoutingConnectionBase(
      MySQLRoutingContext &context,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : context_(context),
        remove_callback_(std::move(remove_callback)),
        started_(std::chrono::system_clock::now()) {}

  virtual ~MySQLRoutingConnectionBase() = default;

  MySQLRoutingContext &context() { return context_; }

  virtual std::string get_destination_id() const = 0;

  /**
   * @brief Returns address of server to which connection is established.
   *
   * @return address of server
   */
  virtual std::string get_server_address() const = 0;

  virtual void disconnect() = 0;

  /**
   * @brief Returns address of client which connected to router
   *
   * @return address of client
   */
  virtual std::string get_client_address() const = 0;

  std::size_t get_bytes_up() const { return bytes_up_; }
  std::size_t get_bytes_down() const { return bytes_down_; }

  using time_point_type = std::chrono::time_point<std::chrono::system_clock>;

  time_point_type get_started() const { return started_; }
  time_point_type get_connected_to_server() const { return connected_server_; }
  time_point_type get_last_sent_to_server() const {
    return last_sent_to_server_;
  }
  time_point_type get_last_received_from_server() const {
    return last_received_from_server_;
  }

  void transfered_to_server(size_t bytes) {
    last_sent_to_server_ = std::chrono::system_clock::now();
    bytes_down_ += bytes;
  }

  void transfered_to_client(size_t bytes) {
    last_received_from_server_ = std::chrono::system_clock::now();
    bytes_up_ += bytes;
  }

  void disassociate() { remove_callback_(this); }

  std::vector<uint8_t> &client_buffer() { return client_buffer_; }
  std::vector<uint8_t> &server_buffer() { return server_buffer_; }

 protected:
  /** @brief wrapper for common data used by all routing threads */
  MySQLRoutingContext &context_;
  /** @brief callback that is called when thread of execution completes */
  std::function<void(MySQLRoutingConnectionBase *)> remove_callback_;

  std::size_t bytes_up_{0};
  std::size_t bytes_down_{0};

  time_point_type started_;
  time_point_type connected_server_;
  time_point_type last_sent_to_server_;
  time_point_type last_received_from_server_;

  std::vector<uint8_t> client_buffer_;
  std::vector<uint8_t> server_buffer_;
};

template <class ClientProtocol, class ServerProtocol>
class Splicer;

/**
 * @brief MySQLRoutingConnection represents a connection to MYSQL Server.
 *
 * The MySQLRoutingConnection wraps thread that handles traffic from one socket
 * to another.
 *
 * When instance of MySQLRoutingConnection object is created, remove callback
 * is passed, which is called when connection thread completes.
 */
template <class ClientProtocol, class ServerProtocol>
class MySQLRoutingConnection : public MySQLRoutingConnectionBase {
 public:
  using client_protocol_type = ClientProtocol;
  using server_protocol_type = ServerProtocol;

  /**
   * @brief Creates and initializes connection object. It doesn't create
   *        new thread of execution. In order to create new thread of
   *        execution call start().
   *
   * @param context wrapper for common data used by all connection threads
   * @param destination_id identifier of the destination connected to
   * @param client_socket socket used to send/receive data to/from client
   * @param client_endpoint address of the socket used to send/receive data
   * to/from client
   * @param server_socket socket used to send/receive data to/from server
   * @param server_endpoint IP address and TCP port of MySQL Server
   * @param remove_callback called when thread finishes its execution to remove
   *        associated MySQLRoutingConnection from container. It must be called
   *        at the very end of thread execution
   */
  MySQLRoutingConnection(
      MySQLRoutingContext &context, std::string destination_id,
      typename ClientProtocol::socket client_socket,
      typename ClientProtocol::endpoint client_endpoint,
      typename ServerProtocol::socket server_socket,
      typename ServerProtocol::endpoint server_endpoint,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : MySQLRoutingConnectionBase{context, remove_callback},
        destination_id_{std::move(destination_id)},
        client_socket_{std::move(client_socket)},
        client_endpoint_{std::move(client_endpoint)},
        server_socket_{std::move(server_socket)},
        server_endpoint_{std::move(server_endpoint)} {}

  std::string get_destination_id() const override { return destination_id_; }

  std::string get_client_address() const override {
    std::ostringstream oss;
    oss << client_endpoint_;
    return oss.str();
  }

  std::string get_server_address() const override {
    std::ostringstream oss;
    oss << server_endpoint_;
    return oss.str();
  }

  void connected() {
    connected_server_ = std::chrono::system_clock::now();

    if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
      log_debug("[%s] fd=%d connected %s -> %s as fd=%d",
                context().get_name().c_str(), client_socket().native_handle(),
                get_client_address().c_str(), get_server_address().c_str(),
                server_socket().native_handle());
    }

    context().increase_info_active_routes();
    context().increase_info_handled_routes();
  }

  void disconnect() override { client_socket_.cancel(); }

  void async_run() {
    std::make_shared<Splicer<ClientProtocol, ServerProtocol>>(
        this, context_.get_net_buffer_length())
        ->async_run();
  }

  typename client_protocol_type::socket &client_socket() {
    return client_socket_;
  }

  typename client_protocol_type::endpoint client_endpoint() const {
    return client_endpoint_;
  }

  typename server_protocol_type::socket &server_socket() {
    return server_socket_;
  }

  typename server_protocol_type::endpoint server_endpoint() const {
    return server_endpoint_;
  }

 private:
  std::string destination_id_;

  typename client_protocol_type::socket client_socket_;
  typename client_protocol_type::endpoint client_endpoint_;

  typename server_protocol_type::socket server_socket_;
  typename server_protocol_type::endpoint server_endpoint_;
};

template <class ClientProtocol, class ServerProtocol>
class Splicer : public std::enable_shared_from_this<
                    Splicer<ClientProtocol, ServerProtocol>> {
 public:
  enum class State {
    SPLICE,
    FINISH,
    DONE,
  };

  using client_protocol = ClientProtocol;
  using server_protocol = ServerProtocol;

  Splicer(MySQLRoutingConnection<ClientProtocol, ServerProtocol> *conn,
          const size_t net_buffer_size)
      : conn_{conn} {
    conn_->client_buffer().resize(net_buffer_size);
    conn_->server_buffer().resize(net_buffer_size);
  }

  ~Splicer() {
    if (state_ != State::DONE) std::terminate();

    conn_->disassociate();
  }

  State copy_client_to_server() {
    // Handle traffic from Client to Server
    auto copy_res = conn_->context().get_protocol().copy_packets(
        conn_->client_socket().native_handle(),
        conn_->server_socket().native_handle(), true, conn_->server_buffer(),
        &pktnr_, handshake_done_, false);
    if (!copy_res) {
      if (copy_res.error() == std::errc::resource_unavailable_try_again) {
        return State::SPLICE;
      } else if (copy_res.error() != net::stream_errc::eof) {
        extra_msg_ =
            "Copy client->server failed: " + copy_res.error().message();
      } else if (!handshake_done_) {
        extra_msg_ = "Copy client->server failed: unexpected connection close";
      }
      // client close on us.
      return State::FINISH;
    } else {
      conn_->transfered_to_server(copy_res.value());
    }

    return State::SPLICE;
  }

  State copy_server_to_client() {
    // Handle traffic from Server to Client
    // Note: In classic protocol Server _always_ talks first
    const auto copy_res = conn_->context().get_protocol().copy_packets(
        conn_->server_socket().native_handle(),
        conn_->client_socket().native_handle(), true, conn_->client_buffer(),
        &pktnr_, handshake_done_, true);

    if (!copy_res) {
      if (copy_res.error() == std::errc::resource_unavailable_try_again) {
        return State::SPLICE;
      } else if (copy_res.error() != net::stream_errc::eof) {
        extra_msg_ =
            "Copy server->client failed: " + copy_res.error().message();
      }

      return State::FINISH;
    } else {
      conn_->transfered_to_client(copy_res.value());
    }

    // after a successful handshake, we reset client-side connection error
    // counter, just like the Server
    if (!error_counter_already_cleared_ && handshake_done_) {
      conn_->context().template clear_error_counter<ClientProtocol>(
          conn_->client_endpoint());
      error_counter_already_cleared_ = true;
    }

    return State::SPLICE;
  }

  template <bool to_server>
  void transfer(std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      if (state() != State::DONE) state(finish());
      return;
    }

    state(to_server ? copy_client_to_server() : copy_server_to_client());

    switch (state()) {
      case State::SPLICE:
        if (to_server) {
          async_wait_client();
        } else {
          async_wait_server();
        }
        break;
      case State::FINISH:
        state(finish());
        break;
      case State::DONE:
        break;
    }
  }

  void async_wait_client() {
    conn_->client_socket().async_wait(
        net::socket_base::wait_read,
        std::bind(&Splicer<ClientProtocol, ServerProtocol>::transfer<true>,
                  this->shared_from_this(), std::placeholders::_1));
  }

  void async_wait_server() {
    conn_->server_socket().async_wait(
        net::socket_base::wait_read,
        std::bind(&Splicer<ClientProtocol, ServerProtocol>::transfer<false>,
                  this->shared_from_this(), std::placeholders::_1));
  }

  void async_run() {
    conn_->connected();

    async_wait_client();
    async_wait_server();
  }

 private:
  void state(State st) { state_ = st; }

  State state() { return state_; }

  State finish() {
    if (!handshake_done_) {
      harness_assert(!error_counter_already_cleared_);

      log_info("[%s] fd=%d Pre-auth socket failure %s: %s",
               conn_->context().get_name().c_str(),
               conn_->client_socket().native_handle(),
               mysqlrouter::to_string(conn_->client_endpoint()).c_str(),
               extra_msg_.c_str());
      conn_->context().template block_client_host<ClientProtocol>(
          conn_->client_endpoint(), conn_->server_socket().native_handle());
    }

    // Either client or server terminated

    log_debug("[%s] fd=%d -- %d: connection closed (up: %zub; down: %zub) %s",
              conn_->context().get_name().c_str(),
              conn_->client_socket().native_handle(),
              conn_->server_socket().native_handle(), conn_->get_bytes_up(),
              conn_->get_bytes_down(), extra_msg_.c_str());

    if (conn_->client_socket().is_open()) {
      conn_->client_socket().shutdown(net::socket_base::shutdown_send);
      conn_->client_socket().close();
    }

    if (conn_->server_socket().is_open()) {
      conn_->server_socket().shutdown(net::socket_base::shutdown_send);

      conn_->server_socket().close();
    }

    conn_->context().decrease_info_active_routes();

    return State::DONE;
  }

  MySQLRoutingConnection<ClientProtocol, ServerProtocol> *conn_;

  bool handshake_done_{false};
  bool error_counter_already_cleared_{false};
  std::string extra_msg_;
  int pktnr_{};

  State state_{State::SPLICE};
};

#endif /* ROUTING_CONNECTION_INCLUDED */
