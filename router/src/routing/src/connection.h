/*
  Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTING_CONNECTION_INCLUDED
#define ROUTING_CONNECTION_INCLUDED

#include <chrono>
#include <cstdint>  // size_t
#include <functional>
#include <memory>
#include <optional>

#include "basic_protocol_splicer.h"
#include "context.h"
#include "destination.h"  // DestinationManager
#include "destination_error.h"
#include "mysql/harness/destination.h"
#include "mysql/harness/destination_endpoint.h"
#include "mysql/harness/destination_socket.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/stdx/expected.h"
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

  virtual std::optional<mysql_harness::Destination> get_destination_id()
      const = 0;

  virtual std::optional<mysql_harness::Destination> read_only_destination_id()
      const {
    return get_destination_id();
  }

  virtual std::optional<mysql_harness::Destination> read_write_destination_id()
      const {
    return get_destination_id();
  }

  virtual std::optional<mysql_harness::DestinationEndpoint>
  destination_endpoint() const = 0;

  virtual std::optional<mysql_harness::DestinationEndpoint>
  read_only_destination_endpoint() const {
    return destination_endpoint();
  }

  virtual std::optional<mysql_harness::DestinationEndpoint>
  read_write_destination_endpoint() const {
    return destination_endpoint();
  }

  virtual net::impl::socket::native_handle_type get_client_fd() const = 0;

  virtual std::string get_routing_source() const = 0;

  virtual void set_routing_source(std::string name) = 0;

  virtual void wait_until_completed() = 0;
  virtual void completed() = 0;

  virtual routing_guidelines::Server_info get_server_info() const = 0;

  /**
   * @brief Returns address of server to which connection is established.
   *
   * @return address of server
   */
  std::string get_server_address() const {
    return stats_([](const Stats &stats) { return stats.server_address; });
  }

  void server_address(const std::string &dest) {
    return stats_([&dest](Stats &stats) { stats.server_address = dest; });
  }

  virtual void disconnect() = 0;

  /**
   * @brief Returns address of client which connected to router
   *
   * @return address of client
   */
  std::string get_client_address() const {
    return stats_([](const Stats &stats) { return stats.client_address; });
  }

  void client_address(const std::string &dest) {
    return stats_([&dest](Stats &stats) { stats.client_address = dest; });
  }

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
    Stats() = default;

    Stats(std::string client_address, std::string server_address,
          std::size_t bytes_up, std::size_t bytes_down, time_point_type started,
          time_point_type connected_to_server,
          time_point_type last_sent_to_server,
          time_point_type last_received_from_server)
        : client_address(std::move(client_address)),
          server_address(std::move(server_address)),
          bytes_up(bytes_up),
          bytes_down(bytes_down),
          started(started),
          connected_to_server(connected_to_server),
          last_sent_to_server(last_sent_to_server),
          last_received_from_server(last_received_from_server) {}

    std::string client_address;
    std::string server_address;

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

  virtual void connected();

  template <class F>
  auto disconnect_request(F &&f) {
    return disconnect_(std::forward<F>(f));
  }

  bool disconnect_requested() const {
    return disconnect_([](auto requested) { return requested; });
  }

  routing_guidelines::Session_info get_session_info() const;

  void set_routing_guidelines_session_rand();

 protected:
  /** @brief wrapper for common data used by all routing threads */
  MySQLRoutingContext &context_;
  /** @brief callback that is called when thread of execution completes */
  std::function<void(MySQLRoutingConnectionBase *)> remove_callback_;

  Monitor<Stats> stats_{{}};

  Monitor<bool> disconnect_{{}};

  void log_connection_summary();

 private:
  net::impl::socket::native_handle_type client_fd_;
  std::optional<double> routing_guidelines_session_rand_;
};

class ConnectorBase {
 public:
  ConnectorBase(net::io_context &io_ctx, MySQLRoutingContext &context,
                DestinationManager *destination_manager)
      : io_ctx_{io_ctx},
        context_{context},
        destination_manager_{destination_manager} {}

  enum class Function {
    kInitDestination,
    kConnectFinish,
  };

  mysql_harness::DestinationSocket &socket() { return server_sock_; }
  mysql_harness::DestinationEndpoint &endpoint() { return server_endpoint_; }

  net::steady_timer &timer() { return connect_timer_; }

  void connect_timed_out(bool v) { connect_timed_out_ = v; }

  bool connect_timed_out() const { return connect_timed_out_; }

  void destination_id(std::optional<mysql_harness::Destination> id) {
    destination_id_ = std::move(id);
  }
  std::optional<mysql_harness::Destination> destination_id() const {
    return destination_id_;
  }

  std::string routing_source() const { return destination_->route_name(); }
  void set_routing_source(std::string name) {
    destination_->set_route_name(std::move(name));
  }

  const routing_guidelines::Server_info &server_info() const {
    return destination_->get_server_info();
  }

  void on_connect_failure(
      std::function<void(const mysql_harness::Destination &, std::error_code)>
          func) {
    on_connect_failure_ = std::move(func);
  }

  void on_connect_success(
      std::function<void(const mysql_harness::Destination &)> func) {
    on_connect_success_ = std::move(func);
  }

  void on_is_destination_good(
      std::function<bool(const mysql_harness::Destination &dest)> func) {
    on_is_destination_good_ = std::move(func);
  }

  bool is_destination_good(const mysql_harness::Destination &dest) const {
    if (on_is_destination_good_) return on_is_destination_good_(dest);

    return true;
  }

 protected:
  stdx::expected<void, std::error_code> resolve();
  stdx::expected<void, std::error_code> init_destination(
      routing_guidelines::Session_info session_info);
  stdx::expected<void, std::error_code> init_endpoint();
  stdx::expected<void, std::error_code> next_endpoint();
  stdx::expected<void, std::error_code> next_destination();
  stdx::expected<void, std::error_code> connect_init();
  stdx::expected<void, std::error_code> try_connect();
  stdx::expected<void, std::error_code> connect_finish();
  stdx::expected<void, std::error_code> connected();
  stdx::expected<void, std::error_code> connect_failed(std::error_code ec);

  net::io_context &io_ctx_;
  MySQLRoutingContext &context_;

  net::ip::tcp::resolver resolver_{io_ctx_};
  mysql_harness::DestinationSocket server_sock_{
      mysql_harness::DestinationSocket::TcpType{io_ctx_}};
  mysql_harness::DestinationEndpoint server_endpoint_;

  routing_guidelines::Session_info session_info_;

  DestinationManager *destination_manager_;
  std::unique_ptr<Destination> destination_{nullptr};
  std::vector<mysql_harness::DestinationEndpoint> endpoints_;
  std::vector<mysql_harness::DestinationEndpoint>::iterator endpoints_it_;

  std::error_code last_ec_{make_error_code(DestinationsErrc::kNotSet)};

  Function func_{Function::kInitDestination};

  net::steady_timer connect_timer_{io_ctx_};

  bool connect_timed_out_{false};
  std::optional<mysql_harness::Destination> destination_id_;

  std::function<void(const mysql_harness::Destination &, std::error_code)>
      on_connect_failure_;
  std::function<void(const mysql_harness::Destination &)> on_connect_success_;
  std::function<bool(const mysql_harness::Destination &)>
      on_is_destination_good_;
};

template <class ConnectionType>
class Connector : public ConnectorBase {
 public:
  using ConnectorBase::ConnectorBase;

  stdx::expected<ConnectionType, std::error_code> connect(
      routing_guidelines::Session_info session_info) {
    switch (func_) {
      case Function::kInitDestination: {
        auto init_res = init_destination(std::move(session_info));
        if (!init_res) return stdx::unexpected(init_res.error());

      } break;
      case Function::kConnectFinish: {
        auto connect_res = connect_finish();
        if (!connect_res) return stdx::unexpected(connect_res.error());

      } break;
    }

    if (!destination_id().has_value()) {
      // stops at 'connect_init()
      {
        auto connect_res = try_connect();
        if (!connect_res) return stdx::unexpected(connect_res.error());
      }
    }
    using ret_type = stdx::expected<ConnectionType, std::error_code>;

    if (socket().is_local()) {
      return ret_type{std::in_place, std::make_unique<UnixDomainConnection>(
                                         std::move(socket().as_local()),
                                         std::move(endpoint().as_local()))};
    }

    return ret_type{std::in_place, std::make_unique<TcpConnection>(
                                       std::move(socket().as_tcp()),
                                       std::move(endpoint().as_tcp()))};
  }
};

#endif /* ROUTING_CONNECTION_INCLUDED */
