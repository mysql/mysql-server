/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
#include <optional>

#include "basic_protocol_splicer.h"
#include "context.h"
#include "destination.h"  // RouteDestination
#include "destination_error.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/stdx/monitor.h"

class MySQLRoutingConnectionBase {
 public:
  MySQLRoutingConnectionBase(
      MySQLRoutingContext &context,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : context_(context), remove_callback_(std::move(remove_callback)) {}

  virtual ~MySQLRoutingConnectionBase() = default;

  MySQLRoutingContext &context() { return context_; }
  const MySQLRoutingContext &context() const { return context_; }

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

  void accepted();

  void connected();

  template <class F>
  auto disconnect_request(F &&f) {
    return disconnect_(std::forward<F>(f));
  }

 protected:
  /** @brief wrapper for common data used by all routing threads */
  MySQLRoutingContext &context_;
  /** @brief callback that is called when thread of execution completes */
  std::function<void(MySQLRoutingConnectionBase *)> remove_callback_;

  Monitor<Stats> stats_{{}};

  Monitor<bool> disconnect_{{}};
};

class ConnectorBase {
 public:
  using server_protocol_type = net::ip::tcp;

  ConnectorBase(net::io_context &io_ctx, RouteDestination *route_destination,
                Destinations &destinations)
      : io_ctx_{io_ctx},
        route_destination_{route_destination},
        destinations_{destinations},
        destinations_it_{destinations_.begin()} {}

  enum class Function {
    kInitDestination,
    kConnectFinish,
  };

  server_protocol_type::socket &socket() { return server_sock_; }
  server_protocol_type::endpoint &endpoint() { return server_endpoint_; }

  net::steady_timer &timer() { return connect_timer_; }

  void connect_timed_out(bool v) { connect_timed_out_ = v; }

  bool connect_timed_out() const { return connect_timed_out_; }

  void destination_id(std::string id) { destination_id_ = id; }
  std::string destination_id() const { return destination_id_; }

  void on_connect_failure(
      std::function<void(std::string, uint16_t, std::error_code)> func) {
    on_connect_failure_ = std::move(func);
  }

  void on_connect_success(std::function<void(std::string, uint16_t)> func) {
    on_connect_success_ = std::move(func);
  }

  void on_is_destination_good(std::function<bool(std::string, uint16_t)> func) {
    on_is_destination_good_ = std::move(func);
  }

  bool is_destination_good(const std::string &hostname, uint16_t port) const {
    if (on_is_destination_good_) return on_is_destination_good_(hostname, port);

    return true;
  }

 protected:
  stdx::expected<void, std::error_code> resolve();
  stdx::expected<void, std::error_code> init_destination();
  stdx::expected<void, std::error_code> init_endpoint();
  stdx::expected<void, std::error_code> next_endpoint();
  stdx::expected<void, std::error_code> next_destination();
  stdx::expected<void, std::error_code> connect_init();
  stdx::expected<void, std::error_code> try_connect();
  stdx::expected<void, std::error_code> connect_finish();
  stdx::expected<void, std::error_code> connected();
  stdx::expected<void, std::error_code> connect_failed(std::error_code ec);

  net::io_context &io_ctx_;

  net::ip::tcp::resolver resolver_{io_ctx_};
  server_protocol_type::socket server_sock_{io_ctx_};
  server_protocol_type::endpoint server_endpoint_;

  RouteDestination *route_destination_;
  Destinations &destinations_;
  Destinations::iterator destinations_it_;
  net::ip::tcp::resolver::results_type endpoints_;
  net::ip::tcp::resolver::results_type::iterator endpoints_it_;

  std::error_code last_ec_{make_error_code(DestinationsErrc::kNotSet)};

  Function func_{Function::kInitDestination};

  net::steady_timer connect_timer_{io_ctx_};

  bool connect_timed_out_{false};
  std::string destination_id_;

  std::function<void(std::string, uint16_t, std::error_code)>
      on_connect_failure_;
  std::function<void(std::string, uint16_t)> on_connect_success_;
  std::function<bool(std::string, uint16_t)> on_is_destination_good_;
};

template <class ConnectionType>
class Connector : public ConnectorBase {
 public:
  using ConnectorBase::ConnectorBase;

  stdx::expected<ConnectionType, std::error_code> connect() {
    switch (func_) {
      case Function::kInitDestination: {
        auto init_res = init_destination();
        if (!init_res) return init_res.get_unexpected();

      } break;
      case Function::kConnectFinish: {
        auto connect_res = connect_finish();
        if (!connect_res) return connect_res.get_unexpected();

      } break;
    }

    if (destination_id().empty()) {
      // stops at 'connect_init()
      {
        auto connect_res = try_connect();
        if (!connect_res) return connect_res.get_unexpected();
      }
    }

    return {std::in_place, std::make_unique<TcpConnection>(
                               std::move(socket()), std::move(endpoint()))};
  }
};

template <class ConnectionType>
class PooledConnector : public ConnectorBase {
 public:
  using pool_lookup_cb = std::function<std::optional<ConnectionType>(
      const server_protocol_type::endpoint &ep)>;

  PooledConnector(net::io_context &io_ctx, RouteDestination *route_destination,
                  Destinations &destinations, pool_lookup_cb pool_lookup)
      : ConnectorBase{io_ctx, route_destination, destinations},
        pool_lookup_{std::move(pool_lookup)} {}

  stdx::expected<ConnectionType, std::error_code> connect() {
    switch (func_) {
      case Function::kInitDestination: {
        auto init_res = init_destination();
        if (!init_res) return init_res.get_unexpected();

      } break;
      case Function::kConnectFinish: {
        auto connect_res = connect_finish();
        if (!connect_res) return connect_res.get_unexpected();

      } break;
    }

    if (destination_id().empty()) {
      // stops at 'connect_init()
      if (auto pool_res = probe_pool()) {
        // return the pooled connection.
        return std::move(pool_res.value());
      }

      {
        auto connect_res = try_connect();
        if (!connect_res) return connect_res.get_unexpected();
      }
    }

    return {std::in_place, std::make_unique<TcpConnection>(
                               std::move(socket()), std::move(endpoint()))};
  }

 private:
  std::optional<ConnectionType> probe_pool() {
    return pool_lookup_(server_endpoint_);
  }

  pool_lookup_cb pool_lookup_;
};

#endif /* ROUTING_CONNECTION_INCLUDED */
