/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "connection.h"

#include <random>
#include <string>
#include <system_error>  // error_code

#include "mysql/harness/destination.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/utils.h"  // to_string

IMPORT_LOG_FUNCTIONS()

stdx::expected<void, std::error_code> ConnectorBase::init_destination(
    routing_guidelines::Session_info session_info) {
  session_info_ = std::move(session_info);

  if (!destination_manager_->init_destinations(session_info_)) {
    return stdx::unexpected(make_error_code(DestinationsErrc::kNoDestinations));
  }

  destination_ = destination_manager_->get_next_destination(session_info_);
  if (!destination_) {
    return stdx::unexpected(make_error_code(DestinationsErrc::kNoDestinations));
  }

  return is_destination_good(destination_->destination()) ? resolve()
                                                          : next_destination();
}

stdx::expected<void, std::error_code> ConnectorBase::resolve() {
  if (destination_->destination().is_tcp()) {
    auto tcp_dest = destination_->destination().as_tcp();

    const auto resolve_res =
        resolver_.resolve(tcp_dest.hostname(), std::to_string(tcp_dest.port()));

    if (!resolve_res) {
      destination_manager_->connect_status(resolve_res.error());

      log_warning("%d: resolve() failed: %s", __LINE__,
                  resolve_res.error().message().c_str());
      return next_destination();
    }

    endpoints_.clear();

    for (const auto &ep : *resolve_res) {
      endpoints_.emplace_back(
          mysql_harness::DestinationEndpoint::TcpType(ep.endpoint()));
    }
  } else {
    endpoints_.clear();

    endpoints_.emplace_back(mysql_harness::DestinationEndpoint::LocalType(
        destination_->destination().as_local().path()));
  }

  return init_endpoint();
}

stdx::expected<void, std::error_code> ConnectorBase::init_endpoint() {
  endpoints_it_ = endpoints_.begin();

  return connect_init();
}

stdx::expected<void, std::error_code> ConnectorBase::connect_init() {
  // close socket if it is already open
  server_sock_.close();

  connect_timed_out(false);

  server_endpoint_ = *endpoints_it_;

  return {};
}

stdx::expected<void, std::error_code> ConnectorBase::try_connect() {
#if 0
  if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
    log_debug("trying %s", mysqlrouter::to_string(server_endpoint_).c_str());
  }
#endif

  const int socket_flags {
#if defined(SOCK_NONBLOCK)
    // linux|freebsd|sol11.4 allows to set NONBLOCK as part of the socket()
    // call to save the extra syscall
    SOCK_NONBLOCK
#endif
  };

  auto open_res = server_sock_.open(server_endpoint_, socket_flags);
  if (!open_res) return stdx::unexpected(open_res.error());

  const auto non_block_res = server_sock_.native_non_blocking(true);
  if (!non_block_res) return stdx::unexpected(non_block_res.error());

  if (server_endpoint_.is_tcp()) {
    server_sock_.set_option(net::ip::tcp::no_delay{true});
  }

#ifdef FUTURE_TASK_USE_SOURCE_ADDRESS
  /* set the source address to take a specific route.
   *
   *
   */

  // IP address of the interface we want to route-through.
  std::string src_addr_str;

  // src_addr_str = "192.168.178.78";

  if (!src_addr_str.empty()) {
    const auto src_addr_res = net::ip::make_address_v4(src_addr_str.c_str());
    if (!src_addr_res) return stdx::unexpected(src_addr_res.error());

#if defined(IP_BIND_ADDRESS_NO_PORT)
    // linux 4.2 introduced IP_BIND_ADDRESS_NO_PORT to delay assigning a
    // source-port until connect()
    net::socket_option::integer<IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT> sockopt;

    const auto setsockopt_res = server_sock_.set_option(sockopt);
    if (!setsockopt_res) {
      // if the glibc supports IP_BIND_ADDRESS_NO_PORT, but the kernel
      // doesn't: ignore it.
      if (setsockopt_res.error() !=
          make_error_code(std::errc::invalid_argument)) {
        log_warning(
            "%d: setsockopt(IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT) "
            "failed: "
            "%s",
            __LINE__, setsockopt_res.error().message().c_str());
        return stdx::unexpected(setsockopt_res.error());
      }
    }
#endif

    const auto bind_res = server_sock_.bind(net::ip::tcp::endpoint(
        src_addr_res.value_or(net::ip::address_v4{}), 0));
    if (!bind_res) return stdx::unexpected(bind_res.error());
  }
#endif

  const auto connect_res = server_sock_.connect(server_endpoint_);
  if (!connect_res) {
    const auto ec = connect_res.error();
    if (ec == make_error_condition(std::errc::operation_in_progress) ||
        ec == make_error_condition(std::errc::operation_would_block)) {
      // connect in progress, wait for completion.
      func_ = Function::kConnectFinish;
      return stdx::unexpected(connect_res.error());
    } else {
      last_ec_ = ec;
      return next_endpoint();
    }
  }

  return connected();
}

stdx::expected<void, std::error_code> ConnectorBase::connect_finish() {
  if (connect_timed_out()) {
    last_ec_ = make_error_code(std::errc::timed_out);

    return next_endpoint();
  }

  net::socket_base::error sock_err;
  const auto getopt_res = server_sock_.get_option(sock_err);

  if (!getopt_res) {
    last_ec_ = getopt_res.error();
    return next_endpoint();
  }

  if (sock_err.value() != 0) {
    std::error_code ec {
      sock_err.value(),
#if defined(_WIN32)
          std::system_category()
#else
          std::generic_category()
#endif
    };

    last_ec_ = ec;

    return next_endpoint();
  }

  return connected();
}

stdx::expected<void, std::error_code> ConnectorBase::connected() {
  destination_id_ = destination_->destination();
  destination_manager_->connect_status({});

  if (on_connect_success_) on_connect_success_(*destination_id_);

  return {};
}

stdx::expected<void, std::error_code> ConnectorBase::next_endpoint() {
  std::advance(endpoints_it_, 1);

  if (endpoints_it_ != endpoints_.end()) {
    return connect_init();
  }

  // report back the connect status to the destination
  destination_manager_->connect_status(last_ec_);

  if (last_ec_ && on_connect_failure_) {
    on_connect_failure_(destination_->destination(), last_ec_);
  }

  return next_destination();
}

stdx::expected<void, std::error_code> ConnectorBase::next_destination() {
  bool is_quarantined{false};
  do {
    destination_ = destination_manager_->get_next_destination(session_info_);
    if (destination_) {
      is_quarantined = !is_destination_good(destination_->destination());
      if (is_quarantined) {
        destination_manager_->connect_status(
            make_error_code(DestinationsErrc::kQuarantined));
      }
    }
  } while (destination_ && is_quarantined);

  if (destination_) {
    // next destination
    return resolve();
  } else if (last_ec_ != make_error_condition(std::errc::timed_out) &&
             last_ec_.category() != net::ip::resolver_category() &&
             destination_manager_->refresh_destinations(session_info_)) {
    // On member failure (connection refused, ...) wait for failover and use
    // the new primary.
    destination_ = destination_manager_->get_next_destination(session_info_);
    if (destination_) {
      return resolve();
    } else {
      // We are done, destination manager should know about that
      destination_manager_->connect_status({});

      // we couldn't connect to any of the destinations. Give up.
      return stdx::unexpected(last_ec_);
    }
  }
  return stdx::unexpected(last_ec_);
}

void MySQLRoutingConnectionBase::accepted() {
  context().increase_info_active_routes();
  context().increase_info_handled_routes();

  client_fd_ = get_client_fd();
}

void MySQLRoutingConnectionBase::connected() {
  stats_([now = clock_type::now()](Stats &stats) {
    stats.connected_to_server = now;
  });

  if (!log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) return;

  const auto stats = get_stats();

  log_debug("[%s] fd=%d connected %s -> %s", context().get_name().c_str(),
            client_fd_, stats.client_address.c_str(),
            stats.server_address.c_str());
}

void MySQLRoutingConnectionBase::log_connection_summary() {
  if (!log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) return;

  auto log_id = [](const std::string &id) -> const char * {
    return id.empty() ? "(not connected)" : id.c_str();
  };

  const auto stats = get_stats();

  log_debug("[%s] fd=%d %s -> %s: connection closed (up: %zub; down: %zub)",
            this->context().get_name().c_str(), client_fd_,
            log_id(stats.client_address), log_id(stats.server_address),
            stats.bytes_up, stats.bytes_down);
}

routing_guidelines::Session_info MySQLRoutingConnectionBase::get_session_info()
    const {
  routing_guidelines::Session_info session_info;
  session_info.target_ip = context_.get_bind_address().hostname();
  session_info.target_port = context_.get_bind_address().port();
  const auto &client_address_res =
      mysql_harness::make_tcp_destination(get_client_address());
  if (client_address_res) {
    session_info.source_ip = client_address_res->hostname();
  } else {
    log_warning(
        "[%s] could not set source IP for routing guidelines evaluation: "
        "'%s'",
        this->context().get_name().c_str(), get_client_address().c_str());
  }

  if (routing_guidelines_session_rand_)
    session_info.random_value = *routing_guidelines_session_rand_;

  return session_info;
}

void MySQLRoutingConnectionBase::set_routing_guidelines_session_rand() {
  std::random_device rd;
  std::mt19937 rng{rd()};
  std::uniform_real_distribution<double> dist(0, 1);
  auto rand = dist(rng);
  routing_guidelines_session_rand_ = rand;
}
