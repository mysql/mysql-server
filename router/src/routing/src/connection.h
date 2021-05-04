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

#ifndef ROUTING_CONNECTION_INCLUDED
#define ROUTING_CONNECTION_INCLUDED

#include <chrono>
#include <cstdint>  // size_t
#include <functional>
#include <memory>
#include <sstream>

#include "context.h"
#include "mysql/harness/stdx/monitor.h"
#include "protocol_splicer.h"

class MySQLRoutingConnectionBase {
 public:
  MySQLRoutingConnectionBase(
      MySQLRoutingContext &context,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : context_(context), remove_callback_(std::move(remove_callback)) {}

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

  std::size_t get_bytes_up() const {
    return stats_([](const Stats &stats) { return stats.bytes_up; });
  }

  std::size_t get_bytes_down() const {
    return stats_([](const Stats &stats) { return stats.bytes_down; });
  }

  using clock_type = std::chrono::system_clock;
  using time_point_type = clock_type::time_point;

  time_point_type get_started() const {
    return stats_([](const Stats &stats) { return stats.started; });
  }

  time_point_type get_connected_to_server() const {
    return stats_([](const Stats &stats) { return stats.connected_to_server; });
  }

  time_point_type get_last_sent_to_server() const {
    return stats_([](const Stats &stats) { return stats.last_sent_to_server; });
  }

  time_point_type get_last_received_from_server() const {
    return stats_(
        [](const Stats &stats) { return stats.last_received_from_server; });
  }

  struct Stats {
    std::size_t bytes_up{0};
    std::size_t bytes_down{0};

    time_point_type started{clock_type::now()};
    time_point_type connected_to_server;
    time_point_type last_sent_to_server;
    time_point_type last_received_from_server;
  };

  Stats get_stats() const {
    return stats_([](const Stats &stats) { return stats; });
  }

  void transfered_to_server(size_t bytes) {
    const auto now = clock_type::now();
    stats_([bytes, now](Stats &stats) {
      stats.last_sent_to_server = now;
      stats.bytes_down += bytes;
    });
  }

  void transfered_to_client(size_t bytes) {
    const auto now = clock_type::now();
    stats_([bytes, now](Stats &stats) {
      stats.last_received_from_server = now;
      stats.bytes_up += bytes;
    });
  }

  void disassociate() { remove_callback_(this); }

 protected:
  /** @brief wrapper for common data used by all routing threads */
  MySQLRoutingContext &context_;
  /** @brief callback that is called when thread of execution completes */
  std::function<void(MySQLRoutingConnectionBase *)> remove_callback_;

  Monitor<Stats> stats_{{}};
};

template <class ClientProtocol, class ServerProtocol>
class Splicer;

/**
 * @brief MySQLRoutingConnection represents a connection to MYSQL Server.
 *
 * The MySQLRoutingConnection wraps thread that handles traffic from one
 * socket to another.
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
   * @param remove_callback called when thread finishes its execution to
   * remove associated MySQLRoutingConnection from container. It must be
   * called at the very end of thread execution
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
    const auto now = clock_type::now();
    stats_([now](Stats &stats) { stats.connected_to_server = now; });

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

#endif /* ROUTING_CONNECTION_INCLUDED */
