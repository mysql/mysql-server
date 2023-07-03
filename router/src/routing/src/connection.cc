/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "connection.h"

#include <string>
#include <system_error>  // error_code

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/utils.h"  // to_string

IMPORT_LOG_FUNCTIONS()

stdx::expected<void, std::error_code> ConnectorBase::init_destination() {
  destinations_it_ = destinations_.begin();

  if (destinations_it_ != destinations_.end()) {
    const auto &destination = *destinations_it_;

    return is_destination_good(destination->hostname(), destination->port())
               ? resolve()
               : next_destination();
  } else {
    // no backends
    log_warning("%d: no connectable destinations :(", __LINE__);
    return stdx::make_unexpected(
        make_error_code(DestinationsErrc::kNoDestinations));
  }
}

stdx::expected<void, std::error_code> ConnectorBase::resolve() {
  const auto &destination = *destinations_it_;

  if (!destination->good()) {
    return next_destination();
  }

  const auto resolve_res = resolver_.resolve(
      destination->hostname(), std::to_string(destination->port()));

  if (!resolve_res) {
    destination->connect_status(resolve_res.error());

    log_warning("%d: resolve() failed: %s", __LINE__,
                resolve_res.error().message().c_str());
    return next_destination();
  }

  endpoints_ = resolve_res.value();

#if 0
  std::cerr << __LINE__ << ": " << destination->hostname() << "\n";
  for (auto const &ep : endpoints_) {
    std::cerr << __LINE__ << ": .. " << ep.endpoint() << "\n";
  }
#endif

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

  auto endpoint = *endpoints_it_;

  server_endpoint_ = endpoint.endpoint();

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

  auto open_res = server_sock_.open(server_endpoint_.protocol(), socket_flags);
  if (!open_res) return open_res.get_unexpected();

  const auto non_block_res = server_sock_.native_non_blocking(true);
  if (!non_block_res) return non_block_res.get_unexpected();

  server_sock_.set_option(net::ip::tcp::no_delay{true});

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
    if (!src_addr_res) return src_addr_res.get_unexpected();

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
        return setsockopt_res.get_unexpected();
      }
    }
#endif

    const auto bind_res = server_sock_.bind(net::ip::tcp::endpoint(
        src_addr_res.value_or(net::ip::address_v4{}), 0));
    if (!bind_res) return bind_res.get_unexpected();
  }
#endif

  const auto connect_res = server_sock_.connect(server_endpoint_);
  if (!connect_res) {
    const auto ec = connect_res.error();
    if (ec == make_error_condition(std::errc::operation_in_progress) ||
        ec == make_error_condition(std::errc::operation_would_block)) {
      // connect in progress, wait for completion.
      func_ = Function::kConnectFinish;
      return connect_res.get_unexpected();
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
  destination_id_ =
      endpoints_it_->host_name() + ":" + endpoints_it_->service_name();

  if (on_connect_success_) {
    on_connect_success_(endpoints_it_->host_name(),
                        endpoints_it_->endpoint().port());
  }

  return {};
}

stdx::expected<void, std::error_code> ConnectorBase::next_endpoint() {
  std::advance(endpoints_it_, 1);

  if (endpoints_it_ != endpoints_.end()) {
    return connect_init();
  } else {
    auto &destination = *destinations_it_;

    // report back the connect status to the destination
    destination->connect_status(last_ec_);

    if (last_ec_ && on_connect_failure_) {
      on_connect_failure_(destination->hostname(), destination->port(),
                          last_ec_);
    }

    return next_destination();
  }
}

stdx::expected<void, std::error_code> ConnectorBase::next_destination() {
  do {
    std::advance(destinations_it_, 1);

    if (destinations_it_ == std::end(destinations_)) break;

    const auto &destination = *destinations_it_;
    if (is_destination_good(destination->hostname(), destination->port())) {
      break;
    }
  } while (true);

  if (destinations_it_ != destinations_.end()) {
    // next destination
    return resolve();
  } else {
    auto refresh_res = route_destination_->refresh_destinations(destinations_);
    if (refresh_res) {
      destinations_ = std::move(refresh_res.value());
      return init_destination();
    } else {
      // we couldn't connect to any of the destinations. Give up.
      return stdx::make_unexpected(last_ec_);
    }
  }
}

void MySQLRoutingConnectionBase::accepted() {
  context().increase_info_active_routes();
  context().increase_info_handled_routes();
}

void MySQLRoutingConnectionBase::connected() {
  const auto now = clock_type::now();
  stats_([now](Stats &stats) { stats.connected_to_server = now; });

  if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
    log_debug("[%s] connected %s -> %s", context().get_name().c_str(),
              get_client_address().c_str(), get_server_address().c_str());
  }
}
