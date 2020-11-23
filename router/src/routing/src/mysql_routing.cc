/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates.

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

#include "mysql_routing.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>  // shared_ptr
#include <mutex>
#include <sstream>  // ostringstream
#include <stdexcept>
#include <system_error>  // error_code
#include <thread>
#include <type_traits>

#include <sys/types.h>

#include "common.h"  // rename_thread
#include "connection.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "dest_next_available.h"
#include "dest_round_robin.h"
#include "destination_ssl_context.h"
#include "hostname_validator.h"
#include "mysql/harness/filesystem.h"  // make_file_private
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/io/file_handle.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/io_thread.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "plugin_config.h"
#include "protocol/base_protocol.h"
#include "protocol/protocol.h"
#include "ssl_mode.h"
#include "tcp_address.h"

using mysqlrouter::string_format;
using routing::AccessMode;
using routing::RoutingStrategy;
IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;

static const int kListenQueueSize{1024};

static const char *kDefaultReplicaSetName = "default";

/**
 * encode a initial error-msg into a buffer.
 *
 * Assumes that no capability exchange happened yet. For classic-protocol that
 * means Error messages will be encoded in 3.23 format.
 *
 * works for error-packets that are encoded by the Acceptor.
 */
static stdx::expected<size_t, std::error_code> encode_initial_error_packet(
    BaseProtocol::Type protocol, std::vector<uint8_t> &error_frame,
    uint32_t error_code, const std::string &msg, const std::string &sql_state) {
  if (protocol == BaseProtocol::Type::kClassicProtocol) {
    return ClassicProtocolSplicer::encode_error_packet(
        error_frame, 0, {}, error_code, msg, sql_state);
  } else {
    return XProtocolSplicer::encode_error_packet(error_frame, error_code, msg,
                                                 sql_state);
  }
}

MySQLRouting::MySQLRouting(
    net::io_context &io_ctx, routing::RoutingStrategy routing_strategy,
    uint16_t port, const Protocol::Type protocol,
    const routing::AccessMode access_mode, const string &bind_address,
    const mysql_harness::Path &named_socket, const string &route_name,
    int max_connections, std::chrono::milliseconds destination_connect_timeout,
    unsigned long long max_connect_errors,
    std::chrono::milliseconds client_connect_timeout,
    unsigned int net_buffer_length, size_t thread_stack_size,
    SslMode client_ssl_mode, TlsServerContext *client_ssl_ctx,
    SslMode server_ssl_mode, DestinationTlsContext *dest_ssl_ctx)
    : context_(protocol, route_name, net_buffer_length,
               destination_connect_timeout, client_connect_timeout,
               mysql_harness::TCPAddress(bind_address, port), named_socket,
               max_connect_errors, thread_stack_size, client_ssl_mode,
               client_ssl_ctx, server_ssl_mode, dest_ssl_ctx),
      io_ctx_{io_ctx},
      routing_strategy_(routing_strategy),
      access_mode_(access_mode),
      max_connections_(set_max_connections(max_connections)),
      service_tcp_(io_ctx_)
#if !defined(_WIN32)
      ,
      service_named_socket_(io_ctx_)
#endif
{
  validate_destination_connect_timeout(destination_connect_timeout);

#ifdef _WIN32
  if (named_socket.is_set()) {
    throw std::invalid_argument(
        "'socket' configuration item is not supported on Windows platform");
  }
#endif

  // This test is only a basic assertion.  Calling code is expected to check the
  // validity of these arguments more thoroughally. At the time of writing,
  // routing_plugin.cc : init() is one such place.
  if (!context_.get_bind_address().port && !named_socket.is_set()) {
    throw std::invalid_argument(
        string_format("No valid address:port (%s:%d) or socket (%s) to bind to",
                      bind_address.c_str(), port, named_socket.c_str()));
  }
}

void MySQLRouting::start(mysql_harness::PluginFuncEnv *env) {
  mysql_harness::rename_thread(
      get_routing_thread_name(context_.get_name(), "RtM")
          .c_str());  // "Rt main" would be too long :(
  if (context_.get_bind_address().port > 0) {
    auto res = setup_tcp_service();
    if (!res) {
      clear_running(env);
      throw std::runtime_error(
          string_format("Setting up TCP service using %s: %s",
                        context_.get_bind_address().str().c_str(),
                        res.error().message().c_str()));
    }

    // routing strategy and mode are mutually-exclusive (mode is legacy)
    if (routing_strategy_ != RoutingStrategy::kUndefined)
      log_info("[%s] started: listening on %s, routing strategy = %s",
               context_.get_name().c_str(),
               context_.get_bind_address().str().c_str(),
               get_routing_strategy_name(routing_strategy_).c_str());
    else
      log_info("[%s] started: listening on %s, routing mode = %s",
               context_.get_name().c_str(),
               context_.get_bind_address().str().c_str(),
               get_access_mode_name(access_mode_).c_str());
  }
#ifndef _WIN32
  if (context_.get_bind_named_socket().is_set()) {
    auto res = setup_named_socket_service();
    if (!res) {
      clear_running(env);
      throw std::runtime_error(
          string_format("Setting up named socket service '%s': %s",
                        context_.get_bind_named_socket().c_str(),
                        res.error().message().c_str()));
    }
    log_info("[%s] started: listening using %s", context_.get_name().c_str(),
             context_.get_bind_named_socket().c_str());
  }
#endif
  if (context_.get_bind_address().port > 0 ||
      context_.get_bind_named_socket().is_set()) {
    start_acceptor(env);
#ifndef _WIN32
    if (context_.get_bind_named_socket().is_set() &&
        unlink(context_.get_bind_named_socket().str().c_str()) == -1) {
      const auto ec = std::error_code{errno, std::generic_category()};
      if (ec != make_error_code(std::errc::no_such_file_or_directory)) {
        log_warning("Failed removing socket file %s (%s %s)",
                    context_.get_bind_named_socket().str().c_str(),
                    ec.message().c_str(), mysqlrouter::to_string(ec).c_str());
      }
    }
#endif
  }
}

class ConnectorBase {
 public:
  enum class State {
    INIT,
    INIT_DESTINATION,
    RESOLVE,
    INIT_ENDPOINT,
    CONNECT,
    CONNECT_FINISH,
    CONNECTED,
    NEXT_ENDPOINT,
    NEXT_DESTINATION,
    DONE,
    ERROR,
  };

  void state(State next_state);

  State state() const { return state_; }

 private:
  State state_{State::INIT};
};

std::ostream &operator<<(std::ostream &os, const ConnectorBase::State &state) {
  using State = ConnectorBase::State;
  switch (state) {
    case State::INIT:
      os << "INIT";
      break;
    case State::INIT_DESTINATION:
      os << "INIT_DESTINATION";
      break;
    case State::RESOLVE:
      os << "RESOLVE";
      break;
    case State::INIT_ENDPOINT:
      os << "INIT_ENDPOINT";
      break;
    case State::CONNECT:
      os << "CONNECT";
      break;
    case State::CONNECT_FINISH:
      os << "CONNECT_FINISH";
      break;
    case State::CONNECTED:
      os << "CONNECTED";
      break;
    case State::NEXT_ENDPOINT:
      os << "NEXT_ENDPOINT";
      break;
    case State::NEXT_DESTINATION:
      os << "NEXT_DESTINATION";
      break;
    case State::DONE:
      os << "DONE";
      break;
    case State::ERROR:
      os << "ERROR";
      break;
  }
  return os;
}

void ConnectorBase::state(State next_state) {
  // log_debug("state: -> %s", mysqlrouter::to_string(next_state).c_str());
  state_ = next_state;
}

/**
 * a simple move-only type to track ownership.
 */
class Owner {
 public:
  Owner() = default;
  Owner(const Owner &) = delete;
  Owner &operator=(const Owner &) = delete;

  Owner(Owner &&rhs) : owns_{std::exchange(rhs.owns_, false)} {}
  Owner &operator=(Owner &&rhs) {
    owns_ = std::exchange(rhs.owns_, false);
    return *this;
  }
  ~Owner() = default;

  /**
   * release ownership.
   */
  void release() { owns_ = false; }

  /**
   * check if still owned.
   */
  operator bool() const { return owns_; }

 private:
  bool owns_{true};
};

/**
 * tries to connect to one of many backends.
 */
template <class ClientProtocol>
class Connector : public ConnectorBase {
 public:
  using client_protocol_type = ClientProtocol;
  using client_socket_type = typename client_protocol_type::socket;
  using client_endpoint_type = typename client_protocol_type::endpoint;

  Connector(MySQLRouting *r, client_socket_type client_sock,
            client_endpoint_type client_endpoint,
            SocketContainer<client_protocol_type> &connector_container)
      : r_{r},
        client_sock_{connector_container.push_back(std::move(client_sock))},
        client_endpoint_{std::move(client_endpoint)},
        connector_container_{connector_container},
        io_ctx_{client_sock_.get_executor().context()},
        resolver_{io_ctx_},
        server_sock_{io_ctx_},
        destinations_{r_->destinations()->destinations()} {}

  Connector(const Connector &) = delete;
  Connector &operator=(const Connector &) = delete;

  Connector(Connector &&rhs) = default;
  Connector &operator=(Connector &&rhs) = default;

  ~Connector() {
    // if the Connector leaves without handing the socket to the
    // MySQLConnection, remove it from the connection container
    if (client_sock_still_owned_) {
      connector_container_.release(client_sock_);
    }
  }

  void operator()(std::error_code ec) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_error("[%s] Failed connecting: %s",
                  r_->get_context().get_name().c_str(), ec.message().c_str());
      }
      return;
    }

    while (true) {
#if 0
      if (client_sock_still_owned_) {
        log_debug("fd=%d state: %s", client_sock_.native_handle(),
                  mysqlrouter::to_string(state()).c_str());
      }
#endif
      switch (state()) {
        case State::INIT:
          state(init());
          break;
        case State::INIT_DESTINATION:
          state(init_destination());
          break;
        case State::RESOLVE:
          state(resolve());
          break;
        case State::INIT_ENDPOINT:
          state(init_endpoint());
          break;
        case State::CONNECT:
          state(connect());

          if (state() == State::CONNECT_FINISH) {
            server_sock_.async_wait(net::socket_base::wait_write,
                                    std::move(*this));
            return;
          }
          break;
        case State::CONNECT_FINISH:
          state(connect_finish());
          break;
        case State::CONNECTED:
          state(connected());
          break;
        case State::NEXT_ENDPOINT:
          state(next_endpoint());
          break;
        case State::NEXT_DESTINATION:
          state(next_destination());
          break;
        case State::ERROR:
          state(error());
          break;
        case State::DONE:
          return;
      }
    }
  }

 private:
  State init() {
    client_sock_.native_non_blocking(true);
    if (std::is_same<client_protocol_type, net::ip::tcp>::value) {
      client_sock_.set_option(net::ip::tcp::no_delay{true});
    }

    return State::INIT_DESTINATION;
  }

  State resolve() {
    const auto &destination = *destinations_it_;

    if (!destination->good()) {
      return State::NEXT_DESTINATION;
    }

    const auto resolve_res = resolver_.resolve(
        destination->hostname(), std::to_string(destination->port()));

    if (!resolve_res) {
      destination->connect_status(resolve_res.error());

      log_warning("%d: resolve() failed: %s", __LINE__,
                  resolve_res.error().message().c_str());
      return State::NEXT_DESTINATION;
    }

    endpoints_ = resolve_res.value();

    return State::INIT_ENDPOINT;
  }

  State init_endpoint() {
    endpoints_it_ = endpoints_.begin();

    return State::CONNECT;
  }

  State connect() {
    // close socket if it is already open
    server_sock_.close();

    auto endpoint = *endpoints_it_;

    if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
      log_debug("fd=%d: trying %s:%s (%s)", client_sock_.native_handle(),
                endpoint.host_name().c_str(), endpoint.service_name().c_str(),
                mysqlrouter::to_string(endpoint.endpoint()).c_str());
    }
    server_endpoint_ = endpoint.endpoint();

    const int socket_flags {
#if defined(SOCK_NONBLOCK)
      // linux|freebsd|sol11.4 allows to set NONBLOCK as part of the socket()
      // call to safe the extra syscall
      SOCK_NONBLOCK
#endif
    };

    auto open_res =
        server_sock_.open(server_endpoint_.protocol(), socket_flags);
    if (!open_res) {
      if (open_res.error() == make_error_code(std::errc::too_many_files_open)) {
        log_warning(
            "%d: opening connection failed due to max-open-files "
            "reached: "
            "%s",
            __LINE__, open_res.error().message().c_str());
      } else {
        log_warning("%d: socket() failed: %s", __LINE__,
                    open_res.error().message().c_str());
      }
      return State::ERROR;
    }
    const auto non_block_res = server_sock_.native_non_blocking(true);
    if (!non_block_res) {
      log_warning("%d: native_non_blocking() failed: %s", __LINE__,
                  non_block_res.error().message().c_str());
      return State::ERROR;
    }

    server_sock_.set_option(net::ip::tcp::no_delay{true});

    std::string src_addr_str;
    // src_addr_str = "192.168.178.78";
    if (!src_addr_str.empty()) {
      const auto src_addr_res = net::ip::make_address_v4(src_addr_str.c_str());
      if (!src_addr_res) {
        log_warning("%d: building src-address from '%s' failed: %s", __LINE__,
                    src_addr_str.c_str(),
                    src_addr_res.error().message().c_str());
        return State::ERROR;
      }

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
        }
        return State::ERROR;
      }
#endif

      const auto bind_res = server_sock_.bind(net::ip::tcp::endpoint(
          src_addr_res.value_or(net::ip::address_v4{}), 0));
      if (!bind_res) {
        log_warning("%d: setting src-address %s failed: %s", __LINE__,
                    src_addr_str.c_str(), bind_res.error().message().c_str());
        return State::ERROR;
      }
    }

    const auto connect_res = server_sock_.connect(server_endpoint_);

    if (connect_res) {
      return State::CONNECTED;
    } else if (connect_res.error() ==
                   make_error_condition(std::errc::operation_in_progress) ||
               connect_res.error() ==
                   make_error_condition(std::errc::operation_would_block)) {
      return State::CONNECT_FINISH;
    } else {
      log_warning("%d: connect(%s) failed: %s - %s", __LINE__,
                  mysqlrouter::to_string(server_endpoint_).c_str(),
                  mysqlrouter::to_string(connect_res.error()).c_str(),
                  connect_res.error().message().c_str());
      // try the next endpoint
      return State::NEXT_ENDPOINT;
    }
  }

  State connect_finish() {
    net::socket_base::error sock_err;
    const auto getopt_res = server_sock_.get_option(sock_err);

    if (!getopt_res) {
      last_ec_ = getopt_res.error();

      return State::NEXT_ENDPOINT;
    }

    if (sock_err.value() != 0) {
#if defined(_WIN32)
      last_ec_ = std::error_code{sock_err.value(), std::system_category()};
#else
      last_ec_ = std::error_code{sock_err.value(), std::generic_category()};
#endif
      return State::NEXT_ENDPOINT;
    }

    return State::CONNECTED;
  }

  State connected() {
    if (!client_sock_still_owned_) throw std::invalid_argument("assert");

    // move the ownership of the client socket from the connector-container to
    // the connection
    client_sock_still_owned_.release();

    // keep the connector-container locked until the socket is added to the
    // container for active connections to alive a race between
    //
    // 1. plugin thread trying to shutdown
    // 2. connector-container gets empty
    // 3. plugin thread sees connectors and connections being empty and shuts
    // down
    // 4. create_connection tries to add connection to
    // active-connection-container

    connector_container_.run([this]() {
      const auto &destination = *destinations_it_;

      r_->create_connection<client_protocol_type, net::ip::tcp>(
          destination->id(),
          connector_container_.release_unlocked(client_sock_), client_endpoint_,
          std::move(server_sock_), server_endpoint_);
    });

    return State::DONE;
  }

  State next_endpoint() {
    std::advance(endpoints_it_, 1);

    if (endpoints_it_ != endpoints_.end()) {
      return State::CONNECT;
    } else {
      auto &destination = *destinations_it_;
      // report back the connect status to the destination
      destination->connect_status(last_ec_);

      return State::NEXT_DESTINATION;
    }
  }

  State next_destination() {
    std::advance(destinations_it_, 1);

    if (destinations_it_ != destinations_.end()) {
      // next destination
      return State::RESOLVE;
    } else {
      auto refresh_res =
          r_->destinations()->refresh_destinations(destinations_);
      if (refresh_res) {
        destinations_ = std::move(refresh_res.value());
        return State::INIT_DESTINATION;
      } else {
        // we couldn't connect to any of the destinations. Give up.
        return State::ERROR;
      }
    }
  }

  State init_destination() {
    // setup first destination
    destinations_it_ = destinations_.begin();

    if (destinations_it_ != destinations_.end()) {
      return State::RESOLVE;
    } else {
      // no backends
      log_warning("%d: no connectable destinations :(", __LINE__);
      return State::ERROR;
    }
  }

  State error() {
    std::vector<uint8_t> error_frame;

    const auto encode_res = encode_initial_error_packet(
        r_->get_context().get_protocol(), error_frame, 2003,
        "Can't connect to remote MySQL server for client connected to '" +
            r_->get_context().get_bind_address().str() + "'",
        "HY000");

    if (!encode_res) {
      log_debug(
          "[%s] fd=%d encode error: %s", r_->get_context().get_name().c_str(),
          client_sock_.native_handle(), encode_res.error().message().c_str());
    } else {
      auto write_res = net::write(client_sock_, net::buffer(error_frame));
      if (!write_res) {
        log_debug(
            "[%s] fd=%d write error: %s", r_->get_context().get_name().c_str(),
            client_sock_.native_handle(), write_res.error().message().c_str());
      }
    }

    // note: tests as checking for this message
    log_warning(
        "Can't connect to remote MySQL server for client connected to '%s'",
        r_->get_context().get_bind_address().str().c_str());
    return State::DONE;
  }

 public:
  void async_run() {
#if 0
    std::vector<std::string> dests;
    for (const auto &dest : destinations_) {
      dests.emplace_back(dest->hostname() + ":" + std::to_string(dest->port()));
    }

    log_debug("destinations: %s", mysql_harness::join(dests, ", ").c_str());
#endif

    // this looks like a no op as the socket should be writable already, but
    // leads to moving the Connector into its io-thread which makes the acceptor
    // thread faster
    client_sock_.async_wait(net::socket_base::wait_write, std::move(*this));
  }

 private:
  friend std::ostream &operator<<(std::ostream &os,
                                  Connector<ClientProtocol>::State &state);
  MySQLRouting *r_;
  client_socket_type &client_sock_;
  client_endpoint_type client_endpoint_;
  SocketContainer<client_protocol_type> &connector_container_;
  Owner client_sock_still_owned_;

  net::io_context &io_ctx_;
  net::ip::tcp::resolver resolver_;
  net::ip::tcp::socket server_sock_;
  net::ip::tcp::endpoint server_endpoint_;
  Destinations destinations_;
  Destinations::iterator destinations_it_;
  net::ip::tcp::resolver::results_type endpoints_;
  net::ip::tcp::resolver::results_type::iterator endpoints_it_;

  std::error_code last_ec_;
};

struct Nothing {};

template <class Protocol>
class Acceptor {
 public:
  using protocol_type = Protocol;
  using socket_type = typename protocol_type::socket;
  using acceptor_socket_type = typename protocol_type::acceptor;
  using acceptor_endpoint_type = typename protocol_type::endpoint;

  Acceptor(MySQLRouting *r, mysql_harness::PluginFuncEnv *env,
           std::list<IoThread> &io_threads,
           acceptor_socket_type &acceptor_socket,
           const acceptor_endpoint_type &acceptor_endpoint,
           SocketContainer<protocol_type> &connector_container,
           WaitableMonitor<Nothing> &waitable)
      : r_(r),
        env_(env),
        io_threads_{io_threads},
        acceptor_socket_(acceptor_socket),
        acceptor_endpoint_{acceptor_endpoint},
        connector_container_{connector_container},
        cur_io_thread_{io_threads_.begin()},
        waitable_{waitable},
        debug_is_logged_{
            log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)} {}

  Acceptor(const Acceptor &) = delete;
  Acceptor(Acceptor &&) = default;

  Acceptor &operator=(const Acceptor &) = delete;
  Acceptor &operator=(Acceptor &&) = default;

  ~Acceptor() {
    if (last_one_) {
      // in case this is the last destructor, notify the waitable that we are
      // finished.
      waitable_.serialize_with_cv([this](auto &, auto &cv) {
        acceptor_socket_.close();
        cv.notify_one();
      });
    }
  }

  void operator()(std::error_code ec) {
    waitable_([this, ec](auto &) {
      if (ec) {
        // TODO(jkneschk): in case we get EMFILE or ENFILE
        //
        // we should continue to accept connections.
        if (ec != std::errc::operation_canceled) {
          log_error("[%s] Failed accepting connection: %s",
                    r_->get_context().get_name().c_str(), ec.message().c_str());
        }
        return;
      }

      while (is_running(env_)) {
        typename protocol_type::endpoint client_endpoint;
        const int socket_flags {
#if defined(SOCK_NONBLOCK)
          // linux|freebsd|sol11.4 allows to set NONBLOCK as part of the
          // accept() call to safe the extra syscall
          SOCK_NONBLOCK
#endif
        };

        auto sock_res = acceptor_socket_.accept(cur_io_thread_->context(),
                                                client_endpoint, socket_flags);
        if (sock_res) {
          // on Linux and AF_UNIX, the client_endpoint will be empty [only
          // family is set]
          //
          // in that case, use the acceptor's endpoint
          if (client_endpoint.size() == 2) {
            client_endpoint = acceptor_endpoint_;
          }

          // round-robin the io-threads for each successfully accepted
          // connection
          cur_io_thread_ = std::next(cur_io_thread_);

          if (cur_io_thread_ == io_threads_.end()) {
            cur_io_thread_ = io_threads_.begin();
          }

          // accepted
          auto sock = std::move(sock_res.value());

#if 0 && defined(SO_INCOMING_CPU)
        // try to run the socket-io on the CPU which also handles the kernels
        // socket-RX queue
        net::socket_option::integer<SOL_SOCKET, SO_INCOMING_CPU>
            incoming_cpu_opt;
        const auto incoming_cpu_res = sock.get_option(incoming_cpu_opt);
        if (incoming_cpu_res) {
          auto affine_cpu = incoming_cpu_opt.value();
          if (affine_cpu >= 0) {
            // if we find a thread that is affine to the RX-queue's CPU, let
            // that io-thread handle it.
            //
            // the incoming CPU may be -1 in case of no affinity.
            for (auto &io_thread : io_threads_) {
              const auto affinity = io_thread.cpu_affinity();

              if (affinity.any() && affinity.test(affine_cpu)) {
                // replace the io-context of the socket
                sock =
                    socket_type(io_thread.context(), client_endpoint.protocol(),
                                sock.release().value());
                break;
              }
            }
          }
        } else if (incoming_cpu_res.error() !=
                   make_error_code(std::errc::invalid_argument)) {
          // ignore there error case where SO_INCOMING_CPU is defined at
          // build-time, but not supported by the kernel at runtime.
          //
          // it was introduced with linux-3.19
          log_info("getsockopt(SOL_SOCKET, SO_INCOMING_CPU) failed: %s",
                   incoming_cpu_res.error().message().c_str());
        }
#endif

          if (debug_is_logged_) {
            if (std::is_same<protocol_type, net::ip::tcp>::value) {
              log_debug("[%s] fd=%d connection accepted at %s",
                        r_->get_context().get_name().c_str(),
                        sock.native_handle(),
                        r_->get_context().get_bind_address().str().c_str());
#ifdef NET_TS_HAS_UNIX_SOCKET
            } else if (std::is_same<protocol_type,
                                    local::stream_protocol>::value) {
#if 0 && !defined(_WIN32)
            // if the messages wouldn't be logged, don't get the peercreds
            pid_t peer_pid;
            uid_t peer_uid;

            // try to be helpful of who tried to connect to use and failed.
            // who == PID + UID
            //
            // if we can't get the PID, we'll just show a simpler errormsg

            if (0 == unix_getpeercred(sock, peer_pid, peer_uid)) {
              log_debug(
                  "[%s] fd=%d connection accepted at %s from (pid=%d, uid=%d)",
                  r_->get_context().get_name().c_str(), sock.native_handle(),
                  r_->get_context().get_bind_named_socket().str().c_str(),
                  peer_pid, peer_uid);
            } else
            // fall through
#endif
              log_debug(
                  "[%s] fd=%d connection accepted at %s",
                  r_->get_context().get_name().c_str(), sock.native_handle(),
                  r_->get_context().get_bind_named_socket().str().c_str());
#endif
            }
          }

          if (r_->get_context().is_blocked<protocol_type>(client_endpoint)) {
            const std::string msg = "Too many connection errors from " +
                                    mysqlrouter::to_string(client_endpoint);

            std::vector<uint8_t> error_frame;
            const auto encode_res =
                encode_initial_error_packet(r_->get_context().get_protocol(),
                                            error_frame, 1129, msg, "HY000");

            if (!encode_res) {
              log_debug("[%s] fd=%d encode error: %s",
                        r_->get_context().get_name().c_str(),
                        sock.native_handle(),
                        encode_res.error().message().c_str());
            } else {
              auto write_res = net::write(sock, net::buffer(error_frame));
              if (!write_res) {
                log_debug("[%s] fd=%d write error: %s",
                          r_->get_context().get_name().c_str(),
                          sock.native_handle(),
                          write_res.error().message().c_str());
              }
            }

            // log_info("%s", msg.c_str());
            sock.close();
          } else if (r_->get_context().info_active_routes_.load(
                         std::memory_order_relaxed) >=
                     r_->get_max_connections()) {
            std::vector<uint8_t> error_frame;
            const auto encode_res = encode_initial_error_packet(
                r_->get_context().get_protocol(), error_frame, 1040,
                "Too many connections to MySQL Router", "08004");

            if (!encode_res) {
              log_debug("[%s] fd=%d encode error: %s",
                        r_->get_context().get_name().c_str(),
                        sock.native_handle(),
                        encode_res.error().message().c_str());
            } else {
              auto write_res = net::write(sock, net::buffer(error_frame));
              if (!write_res) {
                log_debug("[%s] fd=%d write error: %s",
                          r_->get_context().get_name().c_str(),
                          sock.native_handle(),
                          write_res.error().message().c_str());
              }
            }

            sock.close();  // no shutdown() before close()

            log_warning("[%s] reached max active connections (%d max=%d)",
                        r_->get_context().get_name().c_str(),
                        r_->get_context().info_active_routes_.load(),
                        r_->get_max_connections());
          } else {
            Connector<protocol_type>(r_, std::move(sock), client_endpoint,
                                     connector_container_)
                .async_run();
          }
        } else if (sock_res.error() ==
                   make_error_condition(std::errc::operation_would_block)) {
          // nothing more to accept, wait for the next batch
          acceptor_socket_.async_wait(net::socket_base::wait_read,
                                      std::move(*this));
          break;
        } else if (sock_res.error() ==
                   make_error_condition(std::errc::bad_file_descriptor)) {
          // our socket got closed, leave the loop and exit the acceptor
          break;
        } else {
          // something unexpected happened, retry
          log_warning("accepting new connection failed at accept(): %s, %s",
                      mysqlrouter::to_string(sock_res.error()).c_str(),
                      sock_res.error().message().c_str());

          // in case of EMFILE|ENFILE we may want to use a timer to sleep
          // for a while before we start accepting again.

          acceptor_socket_.async_wait(net::socket_base::wait_read,
                                      std::move(*this));
          break;
        }
      }
    });
  }

 private:
  MySQLRouting *r_;
  mysql_harness::PluginFuncEnv *env_;

  std::list<IoThread> &io_threads_;

  acceptor_socket_type &acceptor_socket_;
  const acceptor_endpoint_type &acceptor_endpoint_;
  SocketContainer<protocol_type> &connector_container_;

  std::list<IoThread>::iterator cur_io_thread_;
  WaitableMonitor<Nothing> &waitable_;

  bool debug_is_logged_{};

  /*
   * used to close the socket in the last round of the acceptor:
   *
   * - async_wait(..., std::move(*this));
   *
   * will invoke the move-constructor and destroys the moved-from object, which
   * should be a no-op.
   *
   * In case the acceptor's operator() finishes without calling async_wait(), it
   * exits and it will be destroyed, and close its socket as it is the last-one.
   */
  Owner last_one_;
};

void MySQLRouting::disconnect_all() {
  // close client<->server connections.
  connection_container_.disconnect_all();
}

void MySQLRouting::start_acceptor(mysql_harness::PluginFuncEnv *env) {
  mysql_harness::on_service_ready(env);

  destination_->start(env);
  auto allowed_nodes_changed = [&](const AllowedNodes &nodes,
                                   const std::string &reason) {
    std::ostringstream oss;

    if (!context_.get_bind_address().addr.empty()) {
      oss << context_.get_bind_address().port;
      if (!context_.get_bind_named_socket().str().empty()) oss << " and ";
    }

    if (!context_.get_bind_named_socket().str().empty())
      oss << "named socket " << context_.get_bind_named_socket();

    log_info(
        "Routing %s listening on %s got request to disconnect invalid "
        "connections: %s",
        context_.get_name().c_str(), oss.str().c_str(), reason.c_str());

    // handle allowed nodes changed
    connection_container_.disconnect(nodes);
  };

  allowed_nodes_list_iterator_ =
      destination_->register_allowed_nodes_change_callback(
          allowed_nodes_changed);

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    destination_->unregister_allowed_nodes_change_callback(
        allowed_nodes_list_iterator_);
  });

  // pass the io_threads to the acceptor to distribute new connections across
  // the threads
  auto &io_threads = IoComponent::get_instance().io_threads();

  {
    WaitableMonitor<Nothing> acceptor_waitable{Nothing{}};

    if (tcp_socket().is_open()) {
      tcp_socket().native_non_blocking(true);
      tcp_socket().async_wait(
          net::socket_base::wait_read,
          Acceptor<net::ip::tcp>(this, env, io_threads, tcp_socket(),
                                 service_tcp_endpoint_,
                                 tcp_connector_container_, acceptor_waitable));
    }
#if !defined(_WIN32)
    if (service_named_socket_.is_open()) {
      service_named_socket_.native_non_blocking(true);
      service_named_socket_.async_wait(
          net::socket_base::wait_read,
          Acceptor<local::stream_protocol>(
              this, env, io_threads, service_named_socket_,
              service_named_endpoint_, unix_socket_connector_container_,
              acceptor_waitable));
    }
#endif

    // wait for the signal to shutdown.
    wait_for_stop(env, 0);

    // 1. close and wait for acceptors to close
    // 2. cancel all connectors and wait for them to finish
    // 3. close all connections and wait for them to finish
    //
    acceptor_waitable.wait([this](auto &) {
      if (service_tcp_.is_open()) {
        service_tcp_.cancel();
#if !defined(_WIN32)
      } else if (service_named_socket_.is_open()) {
        service_named_socket_.cancel();
#endif
      } else {
        return true;
      }

      return false;
    });
  }

  // close client sockets which aren't connected to a backend yet
  tcp_connector_container_.disconnect_all();

#if !defined(_WIN32)
  unix_socket_connector_container_.disconnect_all();
#endif

  // wait for connectors to stop
  while (!tcp_connector_container_.empty()
#if !defined(_WIN32)
         || !unix_socket_connector_container_.empty()
#endif
  ) {
    std::this_thread::sleep_for(100ms);
  }

  // disconnect all connections
  disconnect_all();

  // wait until all connections are closed
  {
    std::unique_lock<std::mutex> lk(
        connection_container_.connection_removed_cond_m_);
    connection_container_.connection_removed_cond_.wait(
        lk, [&] { return connection_container_.empty(); });
  }

  log_info("[%s] stopped", context_.get_name().c_str());
}

template <class ClientProtocol, class ServerProtocol>
void MySQLRouting::create_connection(
    const std::string &destination_name,
    typename ClientProtocol::socket client_socket,
    const typename ClientProtocol::endpoint &client_endpoint,
    typename ServerProtocol::socket server_socket,
    const typename ServerProtocol::endpoint &server_endpoint) {
  auto remove_callback = [this](MySQLRoutingConnectionBase *connection) {
    connection_container_.remove_connection(connection);
  };

  auto new_connection =
      std::make_unique<MySQLRoutingConnection<ClientProtocol, ServerProtocol>>(
          context_, destination_name, std::move(client_socket), client_endpoint,
          std::move(server_socket), server_endpoint, remove_callback);

  auto *new_conn_ptr = new_connection.get();

  connection_container_.add_connection(std::move(new_connection));
  new_conn_ptr->async_run();
}

// throws std::runtime_error
/*static*/
void MySQLRouting::set_unix_socket_permissions(const char *socket_file) {
#ifdef _WIN32  // Windows doesn't have Unix sockets
  UNREFERENCED_PARAMETER(socket_file);
#else
  // make sure the socket is accessible to all users
  // NOTE: According to man 7 unix, only r+w access is required to connect to
  // socket, and indeed
  //       setting permissions to rw-rw-rw- seems to work just fine on
  //       Ubuntu 14.04. However, for some reason bind() creates rwxr-xr-x by
  //       default on said system, and Server 5.7 uses rwxrwxrwx for its
  //       socket files. To be compliant with Server, we make our permissions
  //       rwxrwxrwx as well, but the x is probably not necessary.
  bool failed = chmod(socket_file, S_IRUSR | S_IRGRP | S_IROTH |      // read
                                       S_IWUSR | S_IWGRP | S_IWOTH |  // write
                                       S_IXUSR | S_IXGRP | S_IXOTH);  // execute
  if (failed) {
    const auto ec = std::error_code{errno, std::generic_category()};
    std::string msg =
        std::string("Failed setting file permissions on socket file '") +
        socket_file + "': " + ec.message();
    log_error("%s", msg.c_str());
    throw std::runtime_error(msg);
  }
#endif
}

stdx::expected<void, std::error_code> MySQLRouting::setup_tcp_service() {
  net::ip::tcp::resolver resolver(io_ctx_);

  auto resolve_res =
      resolver.resolve(context_.get_bind_address().addr,
                       std::to_string(context_.get_bind_address().port));

  if (!resolve_res) {
    return stdx::make_unexpected(resolve_res.error());
  }

  net::ip::tcp::acceptor sock(io_ctx_);

  stdx::expected<void, std::error_code> last_res =
      stdx::make_unexpected(make_error_code(net::socket_errc::not_found));

  // Try to setup socket and bind
  for (auto const &addr : resolve_res.value()) {
    sock.close();

    last_res = sock.open(addr.endpoint().protocol());
    if (!last_res) {
      log_warning("[%s] setup_tcp_service() error from socket(): %s",
                  context_.get_name().c_str(),
                  last_res.error().message().c_str());
      continue;
    }

    net::socket_base::reuse_address reuse_opt{true};
    last_res = sock.set_option(reuse_opt);
    if (!last_res) {
      log_warning("[%s] setup_tcp_service() error from setsockopt(): %s",
                  context_.get_name().c_str(),
                  last_res.error().message().c_str());
      continue;
    }

    last_res = sock.bind(addr.endpoint());
    if (!last_res) {
      log_warning("[%s] setup_tcp_service() error from bind(): %s",
                  context_.get_name().c_str(),
                  last_res.error().message().c_str());
      continue;
    }

    last_res = sock.listen(kListenQueueSize);
    if (!last_res) {
      // bind() succeeded, but listen() failed: don't retry.
      return stdx::make_unexpected(last_res.error());
    }

    service_tcp_endpoint_ = addr.endpoint();
    service_tcp_ = std::move(sock);

    return {};
  }

  return stdx::make_unexpected(last_res.error());
}

#ifndef _WIN32
stdx::expected<void, std::error_code>
MySQLRouting::setup_named_socket_service() {
  const auto socket_file = context_.get_bind_named_socket().str();

  local::stream_protocol::acceptor sock(io_ctx_);
  auto last_res = sock.open();
  if (!last_res) {
    return stdx::make_unexpected(last_res.error());
  }

  local::stream_protocol::endpoint ep(socket_file);

  last_res = sock.bind(ep);
  if (!last_res) {
    if (last_res.error() != make_error_code(std::errc::address_in_use)) {
      return stdx::make_unexpected(last_res.error());
    }
    // file exists, try to connect to it to see if the socket is already in
    // use

    local::stream_protocol::socket client_sock(io_ctx_);
    auto connect_res = client_sock.connect(ep);
    if (connect_res) {
      log_error("Socket file %s already in use by another process",
                socket_file.c_str());

      return stdx::make_unexpected(
          make_error_code(std::errc::already_connected));
    } else if (connect_res.error() ==
               make_error_code(std::errc::connection_refused)) {
      log_warning(
          "Socket file %s already exists, but seems to be unused. "
          "Deleting and retrying...",
          socket_file.c_str());

      if (unlink(socket_file.c_str()) == -1) {
        const auto ec = std::error_code{errno, std::generic_category()};
        if (ec != make_error_code(std::errc::no_such_file_or_directory)) {
          std::string errmsg = "Failed removing socket file " + socket_file +
                               " (" + ec.message() + " (" +
                               mysqlrouter::to_string(ec) + "))";

          log_warning("%s", errmsg.c_str());
          return stdx::make_unexpected(ec);
        }
      }

      last_res = sock.bind(ep);
      if (!last_res) {
        return stdx::make_unexpected(last_res.error());
      }
    }
  }

  try {
    mysql_harness::make_file_public(socket_file);
  } catch (const std::system_error &ec) {
    return stdx::make_unexpected(ec.code());
  } catch (const std::exception &e) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  last_res = sock.listen(kListenQueueSize);
  if (!last_res) {
    return stdx::make_unexpected(last_res.error());
  }

  service_named_socket_ = std::move(sock);
  service_named_endpoint_ = ep;

  return {};
}
#endif

void MySQLRouting::set_destinations_from_uri(const mysqlrouter::URI &uri) {
  if (uri.scheme == "metadata-cache") {
    // Syntax:
    // metadata_cache://[<metadata_cache_key(unused)>]/<replicaset_name>?role=PRIMARY|SECONDARY|PRIMARY_AND_SECONDARY
    std::string replicaset_name = kDefaultReplicaSetName;

    if (uri.path.size() > 0 && !uri.path[0].empty())
      replicaset_name = uri.path[0];

    destination_ = std::make_unique<DestMetadataCacheGroup>(
        io_ctx_, uri.host, replicaset_name, routing_strategy_, uri.query,
        context_.get_protocol(), access_mode_);
  } else {
    throw std::runtime_error(string_format(
        "Invalid URI scheme; expecting: 'metadata-cache' is: '%s'",
        uri.scheme.c_str()));
  }
}

namespace {

routing::RoutingStrategy get_default_routing_strategy(
    const routing::AccessMode access_mode) {
  switch (access_mode) {
    case routing::AccessMode::kReadOnly:
      return routing::RoutingStrategy::kRoundRobin;
    case routing::AccessMode::kReadWrite:
      return routing::RoutingStrategy::kFirstAvailable;
    default:;  // fall-through
  }

  // safe default if access_mode is also not specified
  return routing::RoutingStrategy::kFirstAvailable;
}

std::unique_ptr<RouteDestination> create_standalone_destination(
    net::io_context &io_ctx, const routing::RoutingStrategy strategy,
    const Protocol::Type protocol, size_t thread_stack_size) {
  switch (strategy) {
    case RoutingStrategy::kFirstAvailable:
      return std::make_unique<DestFirstAvailable>(io_ctx, protocol);
    case RoutingStrategy::kNextAvailable:
      return std::make_unique<DestNextAvailable>(io_ctx, protocol);
    case RoutingStrategy::kRoundRobin:
      return std::make_unique<DestRoundRobin>(io_ctx, protocol,
                                              thread_stack_size);
    case RoutingStrategy::kUndefined:
    case RoutingStrategy::kRoundRobinWithFallback:;  // unsupported, fall
                                                     // through
  }

  throw std::runtime_error("Wrong routing strategy " +
                           std::to_string(static_cast<int>(strategy)));
}
}  // namespace

void MySQLRouting::set_destinations_from_csv(const string &csv) {
  std::stringstream ss(csv);
  std::string part;
  std::pair<std::string, uint16_t> info;

  // if no routing_strategy is defined for standalone routing
  // we set the default based on the mode
  if (routing_strategy_ == RoutingStrategy::kUndefined) {
    routing_strategy_ = get_default_routing_strategy(access_mode_);
  }

  destination_ = create_standalone_destination(
      io_ctx_, routing_strategy_, context_.get_protocol(),
      context_.get_thread_stack_size());

  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    const auto info = mysqlrouter::split_addr_port(part);

    if (mysql_harness::is_valid_domainname(info.first)) {
      mysql_harness::TCPAddress addr(
          info.first, info.second == 0
                          ? Protocol::get_default_port(context_.get_protocol())
                          : info.second);

      destination_->add(addr);
    } else {
      throw std::runtime_error(
          string_format("Destination address '%s' is invalid", part.c_str()));
    }
  }

  // Check whether bind address is part of list of destinations
  for (auto &it : *(destination_)) {
    if (it == context_.get_bind_address()) {
      throw std::runtime_error("Bind Address can not be part of destinations");
    }
  }

  if (destination_->size() == 0) {
    throw std::runtime_error("No destinations available");
  }
}

void MySQLRouting::validate_destination_connect_timeout(
    std::chrono::milliseconds timeout) {
  if (timeout <= std::chrono::milliseconds::zero()) {
    std::string error_msg("[" + context_.get_name() +
                          "] tried to set destination_connect_timeout using "
                          "invalid value, was " +
                          std::to_string(timeout.count()) + " ms");
    throw std::invalid_argument(error_msg);
  }
}

int MySQLRouting::set_max_connections(int maximum) {
  if (maximum <= 0 || maximum > UINT16_MAX) {
    auto err = string_format(
        "[%s] tried to set max_connections using invalid value, was '%d'",
        context_.get_name().c_str(), maximum);
    throw std::invalid_argument(err);
  }
  max_connections_ = maximum;
  return max_connections_;
}

routing::AccessMode MySQLRouting::get_mode() const { return access_mode_; }

routing::RoutingStrategy MySQLRouting::get_routing_strategy() const {
  return routing_strategy_;
}

std::vector<mysql_harness::TCPAddress> MySQLRouting::get_destinations() const {
  return destination_->get_destinations();
}

std::vector<MySQLRoutingAPI::ConnData> MySQLRouting::get_connections() {
  return connection_container_.get_all_connections_info();
}
