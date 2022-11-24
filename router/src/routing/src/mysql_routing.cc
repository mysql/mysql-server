/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef _WIN32
#include <sys/stat.h>  // chmod
#endif

#include "classic_connection.h"
#include "connection.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "dest_next_available.h"
#include "dest_round_robin.h"
#include "destination_ssl_context.h"
#include "hostname_validator.h"
#include "my_thread.h"                 // my_thread_self_setname
#include "mysql/harness/filesystem.h"  // make_file_private
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/executor.h"  // defer
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/io/file_handle.h"
#include "mysql/harness/string_utils.h"  // trim
#include "mysql/harness/tls_server_context.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/io_thread.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // to_string
#include "plugin_config.h"
#include "protocol/base_protocol.h"
#include "protocol/protocol.h"
#include "scope_guard.h"
#include "ssl_mode.h"
#include "tcp_address.h"
#include "x_connection.h"

using mysql_harness::utility::string_format;
using routing::AccessMode;
using routing::RoutingStrategy;
IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;

static const int kListenQueueSize{1024};

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
    return MysqlRoutingClassicConnection::encode_error_packet(
        error_frame, 0, {}, error_code, msg, sql_state);
  } else {
    return MysqlRoutingXConnection::encode_error_packet(error_frame, error_code,
                                                        msg, sql_state);
  }
}

MySQLRouting::MySQLRouting(
    net::io_context &io_ctx, routing::RoutingStrategy routing_strategy,
    uint16_t port, const Protocol::Type protocol,
    const routing::AccessMode access_mode, const std::string &bind_address,
    const mysql_harness::Path &named_socket, const std::string &route_name,
    int max_connections, std::chrono::milliseconds destination_connect_timeout,
    unsigned long long max_connect_errors,
    std::chrono::milliseconds client_connect_timeout,
    unsigned int net_buffer_length, SslMode client_ssl_mode,
    TlsServerContext *client_ssl_ctx, SslMode server_ssl_mode,
    DestinationTlsContext *dest_ssl_ctx, bool connection_sharing,
    std::chrono::milliseconds connection_sharing_delay)
    : context_(protocol, route_name, net_buffer_length,
               destination_connect_timeout, client_connect_timeout,
               mysql_harness::TCPAddress(bind_address, port), named_socket,
               max_connect_errors, client_ssl_mode, client_ssl_ctx,
               server_ssl_mode, dest_ssl_ctx, connection_sharing,
               connection_sharing_delay),
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
  // validity of these arguments more thoroughly. At the time of writing,
  // routing_plugin.cc : init() is one such place.
  if (!context_.get_bind_address().port() && !named_socket.is_set()) {
    throw std::invalid_argument(
        string_format("No valid address:port (%s:%d) or socket (%s) to bind to",
                      bind_address.c_str(), port, named_socket.c_str()));
  }
}

void MySQLRouting::run(mysql_harness::PluginFuncEnv *env) {
  my_thread_self_setname(get_routing_thread_name(context_.get_name(), "RtM")
                             .c_str());  // "Rt main" would be too long :(
  if (context_.get_bind_address().port() > 0) {
    // routing strategy and mode are mutually-exclusive (mode is legacy)
    if (routing_strategy_ != RoutingStrategy::kUndefined)
      log_info("[%s] started: routing strategy = %s",
               context_.get_name().c_str(),
               get_routing_strategy_name(routing_strategy_).c_str());
    else
      log_info("[%s] started: routing mode = %s", context_.get_name().c_str(),
               get_access_mode_name(access_mode_).c_str());
  }
#ifndef _WIN32
  if (context_.get_bind_named_socket().is_set()) {
    auto res = setup_named_socket_service();
    if (!res) {
      clear_running(env);
      throw std::runtime_error(
          string_format("Failed setting up named socket service '%s': %s",
                        context_.get_bind_named_socket().c_str(),
                        res.error().message().c_str()));
    }
    log_info("[%s] started: listening using %s", context_.get_name().c_str(),
             context_.get_bind_named_socket().c_str());
  }
#endif
  if (context_.get_bind_address().port() > 0 ||
      context_.get_bind_named_socket().is_set()) {
    auto res = run_acceptor(env);
    if (!res) {
      clear_running(env);
      throw std::runtime_error(
          string_format("Failed setting up TCP service using %s: %s",
                        context_.get_bind_address().str().c_str(),
                        res.error().message().c_str()));
    }
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

template <class Protocol>
class Acceptor {
 public:
  using client_protocol_type = Protocol;
  using socket_type = typename client_protocol_type::socket;
  using acceptor_socket_type = typename client_protocol_type::acceptor;
  using acceptor_endpoint_type = typename client_protocol_type::endpoint;

  using server_protocol_type = net::ip::tcp;

  Acceptor(MySQLRouting *r, std::list<IoThread> &io_threads,
           acceptor_socket_type &acceptor_socket,
           const acceptor_endpoint_type &acceptor_endpoint,
           WaitableMonitor<Nothing> &waitable)
      : r_(r),
        io_threads_{io_threads},
        acceptor_socket_(acceptor_socket),
        acceptor_endpoint_{acceptor_endpoint},
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
        cv.notify_all();
      });
    }
  }

  template <typename S>
  void graceful_shutdown(S &sock) {
    sock->shutdown(net::socket_base::shutdown_send);
    // we want to capture the socket shared_ptr by value to make sure it lives
    // when the async handler gets executed
    sock->async_wait(net::socket_base::wait_read,
                     [=](auto /*ec*/) { sock->close(); });
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

      auto &routing_component = MySQLRoutingComponent::get_instance();
      while (r_->is_running()) {
        typename client_protocol_type::endpoint client_endpoint;
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
          auto sock =
              std::make_shared<socket_type>(std::move(sock_res.value()));

#if 0 && defined(SO_INCOMING_CPU)
        // try to run the socket-io on the CPU which also handles the kernels
        // socket-RX queue
        net::socket_option::integer<SOL_SOCKET, SO_INCOMING_CPU>
            incoming_cpu_opt;
        const auto incoming_cpu_res = sock->get_option(incoming_cpu_opt);
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
                    std::make_shared<socket_type>(socket_type(io_thread.context(), 
                                client_endpoint.protocol(), sock->release().value()));
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
            if (std::is_same<client_protocol_type, net::ip::tcp>::value) {
              log_debug("[%s] fd=%d connection accepted at %s",
                        r_->get_context().get_name().c_str(),
                        sock->native_handle(),
                        r_->get_context().get_bind_address().str().c_str());
#ifdef NET_TS_HAS_UNIX_SOCKET
            } else if (std::is_same<client_protocol_type,
                                    local::stream_protocol>::value) {
#if 0 && !defined(_WIN32)
            // if the messages wouldn't be logged, don't get the peercreds
            pid_t peer_pid;
            uid_t peer_uid;

            // try to be helpful of who tried to connect to use and failed.
            // who == PID + UID
            //
            // if we can't get the PID, we'll just show a simpler errormsg

            if (0 == unix_getpeercred(*sock, peer_pid, peer_uid)) {
              log_debug(
                  "[%s] fd=%d connection accepted at %s from (pid=%d, uid=%d)",
                  r_->get_context().get_name().c_str(), sock->native_handle(),
                  r_->get_context().get_bind_named_socket().str().c_str(),
                  peer_pid, peer_uid);
            } else
            [[fallthrough]];
#endif
              log_debug(
                  "[%s] fd=%d connection accepted at %s",
                  r_->get_context().get_name().c_str(), sock->native_handle(),
                  r_->get_context().get_bind_named_socket().str().c_str());
#endif
            }
          }

          if (r_->get_context().blocked_endpoints().is_blocked(
                  client_endpoint)) {
            const std::string msg = "Too many connection errors from " +
                                    mysqlrouter::to_string(client_endpoint);

            std::vector<uint8_t> error_frame;
            const auto encode_res =
                encode_initial_error_packet(r_->get_context().get_protocol(),
                                            error_frame, 1129, msg, "HY000");

            if (!encode_res) {
              log_debug("[%s] fd=%d encode error: %s",
                        r_->get_context().get_name().c_str(),
                        sock->native_handle(),
                        encode_res.error().message().c_str());
            } else {
              auto write_res = net::write(*sock, net::buffer(error_frame));
              if (!write_res) {
                log_debug("[%s] fd=%d write error: %s",
                          r_->get_context().get_name().c_str(),
                          sock->native_handle(),
                          write_res.error().message().c_str());
              }
            }

            // log_info("%s", msg.c_str());
            graceful_shutdown(sock);
          } else {
            const auto current_total_connections =
                routing_component.current_total_connections();
            const auto max_total_connections =
                routing_component.max_total_connections();

            const bool max_route_connections_limit_reached =
                r_->get_max_connections() > 0 &&
                r_->get_context().info_active_routes_.load(
                    std::memory_order_relaxed) >= r_->get_max_connections();
            const bool max_total_connections_limit_reached =
                current_total_connections >= max_total_connections;

            if (max_route_connections_limit_reached ||
                max_total_connections_limit_reached) {
              std::vector<uint8_t> error_frame;
              const auto encode_res = encode_initial_error_packet(
                  r_->get_context().get_protocol(), error_frame, 1040,
                  "Too many connections to MySQL Router", "08004");

              if (!encode_res) {
                log_debug("[%s] fd=%d encode error: %s",
                          r_->get_context().get_name().c_str(),
                          sock->native_handle(),
                          encode_res.error().message().c_str());
              } else {
                auto write_res = net::write(*sock, net::buffer(error_frame));
                if (!write_res) {
                  log_debug("[%s] fd=%d write error: %s",
                            r_->get_context().get_name().c_str(),
                            sock->native_handle(),
                            write_res.error().message().c_str());
                }
              }
              graceful_shutdown(sock);
              if (max_route_connections_limit_reached) {
                log_warning(
                    "[%s] reached max active connections for route (%d max=%d)",
                    r_->get_context().get_name().c_str(),
                    r_->get_context().info_active_routes_.load(),
                    r_->get_max_connections());
              } else {
                log_warning("[%s] Total connections count=%" PRIu64
                            " exceeds [DEFAULT].max_total_connections=%" PRIu64,
                            r_->get_context().get_name().c_str(),
                            current_total_connections, max_total_connections);
              }
            } else {
              if (std::is_same_v<Protocol, net::ip::tcp>) {
                sock->set_option(net::ip::tcp::no_delay{true});
              }
              r_->create_connection<client_protocol_type>(std::move(*sock),
                                                          client_endpoint);
            }
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

  std::list<IoThread> &io_threads_;

  acceptor_socket_type &acceptor_socket_;
  const acceptor_endpoint_type &acceptor_endpoint_;

  std::list<IoThread>::iterator cur_io_thread_;
  WaitableMonitor<Nothing> &waitable_;

  bool debug_is_logged_{};

  /*
   * used to close the socket in the last round of the acceptor:
   *
   * - async_wait(..., std::move(*this));
   *
   * will invoke the move-constructor and destroys the moved-from object,
   * which should be a no-op.
   *
   * In case the acceptor's operator() finishes without calling async_wait(),
   * it exits and it will be destroyed, and close its socket as it is the
   * last-one.
   */
  Owner last_one_;
};

void MySQLRouting::disconnect_all() {
  // close client<->server connections.
  connection_container_.disconnect_all();
}

std::string MySQLRouting::get_port_str() const {
  std::string port_str;
  if (!context_.get_bind_address().address().empty() &&
      context_.get_bind_address().port() > 0) {
    port_str += std::to_string(context_.get_bind_address().port());
    if (!context_.get_bind_named_socket().str().empty()) {
      port_str += " and ";
    }
  }
  if (!context_.get_bind_named_socket().str().empty()) {
    port_str += "named socket ";
    port_str += context_.get_bind_named_socket().str();
  }
  return port_str;
}

stdx::expected<void, std::error_code> MySQLRouting::run_acceptor(
    mysql_harness::PluginFuncEnv *env) {
  destination_->start(env);
  destination_->register_start_router_socket_acceptor(
      [this]() { return start_accepting_connections(); });
  destination_->register_stop_router_socket_acceptor(
      [this]() { stop_socket_acceptors(); });
  destination_->register_query_quarantined_destinations(
      [this](const mysql_harness::TCPAddress &addr) -> bool {
        return get_context().shared_quarantine().is_quarantined(addr);
      });
  destination_->register_md_refresh_callback(
      [this](const bool nodes_changed_on_md_refresh,
             const AllowedNodes &nodes) {
        get_context().shared_quarantine().refresh(
            get_context().get_id(), nodes_changed_on_md_refresh, nodes);
      });

  // make sure to stop the acceptors in case of possible exceptions, otherwise
  // we can deadlock the process
  Scope_guard stop_acceptors_guard([&]() { stop_socket_acceptors(); });

  if (!destinations()->empty() ||
      (routing_strategy_ == RoutingStrategy::kFirstAvailable &&
       is_destination_standalone_)) {
    // For standalone destination with first-available strategy we always try
    // to open a listening socket, even if there are no destinations.
    auto res = start_accepting_connections();
    // If the routing started at the exact moment as when the metadata had it
    // initial refresh then it may start the acceptors even if metadata do not
    // allow for it to happen, in that case we pass that information to the
    // destination, socket acceptor state should be handled basend on the
    // destination type.
    if (!is_destination_standalone_) destination_->handle_sockets_acceptors();
    // If we failed to start accepting connections on startup then router
    // should fail.
    if (!res) return res.get_unexpected();
  }
  mysql_harness::on_service_ready(env);

  auto allowed_nodes_changed = [&](const AllowedNodes
                                       &existing_connections_nodes,
                                   const AllowedNodes &new_connection_nodes,
                                   const bool disconnect,
                                   const std::string &disconnect_reason) {
    const std::string &port_str = get_port_str();

    if (disconnect) {
      // handle allowed nodes changed for existing connections
      const auto num_of_cons =
          connection_container_.disconnect(existing_connections_nodes);

      if (num_of_cons > 0) {
        log_info(
            "Routing %s listening on %s got request to disconnect %u invalid "
            "connections: %s",
            context_.get_name().c_str(), port_str.c_str(), num_of_cons,
            disconnect_reason.c_str());
      }
    }

    if (!is_running()) return;
    if (service_tcp_.is_open() && new_connection_nodes.empty()) {
      stop_socket_acceptors();
    } else if (!service_tcp_.is_open() && !new_connection_nodes.empty()) {
      if (!start_accepting_connections()) {
        // We could not start acceptor (e.g. the port is used by other app)
        // In that case we should retry on the next md refresh with the
        // latest instance information.
        destination_->handle_sockets_acceptors();
      }
    }
  };

  allowed_nodes_list_iterator_ =
      destination_->register_allowed_nodes_change_callback(
          allowed_nodes_changed);

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    destination_->unregister_allowed_nodes_change_callback(
        allowed_nodes_list_iterator_);
    destination_->unregister_start_router_socket_acceptor();
    destination_->unregister_stop_router_socket_acceptor();
    destination_->unregister_query_quarantined_destinations();
    destination_->unregister_md_refresh_callback();
  });

  // wait for the signal to shutdown.
  mysql_harness::wait_for_stop(env, 0);
  is_running_ = false;
  get_context().shared_quarantine().stop();

  stop_acceptors_guard.commit();
  // routing is no longer running, lets close listening socket
  stop_socket_acceptors();

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
  return {};
}

stdx::expected<void, std::error_code>
MySQLRouting::restart_accepting_connections() {
  const auto result = start_accepting_connections();

  // if we failed to restart the acceptor we keep retrying every 1 second if we
  // have standalone destination. For the metadata-cache destinations there is
  // another mechanism for that,` that uses metadata TTL as a trigger for that.
  if (is_destination_standalone_ && !result) {
    accept_port_reopen_retry_timer_.cancel();
    accept_port_reopen_retry_timer_.expires_after(1s);
    accept_port_reopen_retry_timer_.async_wait(
        [this](const std::error_code &ec) {
          if (ec && ec == std::errc::operation_canceled) {
            return;
          }
          restart_accepting_connections();
        });
  }

  return result;
}

stdx::expected<void, std::error_code>
MySQLRouting::start_accepting_connections() {
  if (!is_running()) {
    return stdx::make_unexpected(
        std::make_error_code(std::errc::connection_aborted));
  }

  stdx::expected<void, std::error_code> setup_res;
  const bool acceptor_already_running =
      !acceptor_waitable_.serialize_with_cv([this, &setup_res](auto &, auto &) {
        if (!service_tcp_.is_open()) {
          setup_res = this->setup_tcp_service();
          return true;
        }
        return false;
      });
  if (acceptor_already_running) return {};
  if (!setup_res) return setup_res.get_unexpected();

  log_info("Start accepting connections for routing %s listening on %s",
           context_.get_name().c_str(), get_port_str().c_str());
  // pass the io_threads to the acceptor to distribute new connections across
  // the threads
  auto &io_threads = IoComponent::get_instance().io_threads();

  {
    if (tcp_socket().is_open()) {
      tcp_socket().native_non_blocking(true);
      tcp_socket().async_wait(
          net::socket_base::wait_read,
          Acceptor<net::ip::tcp>(this, io_threads, tcp_socket(),
                                 service_tcp_endpoint_, acceptor_waitable_));
    }
#if !defined(_WIN32)
    if (service_named_socket_.is_open()) {
      service_named_socket_.native_non_blocking(true);
      service_named_socket_.async_wait(
          net::socket_base::wait_read,
          Acceptor<local::stream_protocol>(
              this, io_threads, service_named_socket_, service_named_endpoint_,
              acceptor_waitable_));
    }
#endif
  }
  return {};
}

void MySQLRouting::stop_socket_acceptors() {
  // When using a static routing with first-available policy we are never
  // supposed to shut down the accepting socket
  if (is_running() && is_destination_standalone_ &&
      routing_strategy_ == routing::RoutingStrategy::kFirstAvailable)
    return;

  log_info("Stop accepting connections for routing %s listening on %s",
           context_.get_name().c_str(), get_port_str().c_str());
  // 1. close and wait for acceptors to close
  // 2. cancel all connectors and wait for them to finish
  // 3. close all connections and wait for them to finish
  //
  acceptor_waitable_.wait([this](auto &) {
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

template <class ClientProtocol>
void MySQLRouting::create_connection(
    typename ClientProtocol::socket client_socket,
    const typename ClientProtocol::endpoint &client_endpoint) {
  auto remove_callback = [this](MySQLRoutingConnectionBase *connection) {
    connection->context().decrease_info_active_routes();

    connection_container_.remove_connection(connection);
  };

  net::io_context &io_ctx = client_socket.get_executor().context();

  switch (context_.get_protocol()) {
    case BaseProtocol::Type::kClassicProtocol: {
      auto new_connection = MysqlRoutingClassicConnection::create(
          context_, destinations(),
          std::make_unique<BasicConnection<ClientProtocol>>(
              std::move(client_socket), client_endpoint),
          std::make_unique<RoutingConnection<ClientProtocol>>(client_endpoint),
          remove_callback);
      auto *new_conn_ptr = new_connection.get();

      connection_container_.add_connection(std::move(new_connection));

      // defer the call and accept the next connection.
      net::defer(io_ctx, [new_conn_ptr]() { new_conn_ptr->async_run(); });
    } break;
    case BaseProtocol::Type::kXProtocol: {
      auto new_connection = MysqlRoutingXConnection::create(
          context_, destinations(),
          std::make_unique<BasicConnection<ClientProtocol>>(
              std::move(client_socket), client_endpoint),
          std::make_unique<RoutingConnection<ClientProtocol>>(client_endpoint),
          remove_callback);
      auto *new_conn_ptr = new_connection.get();

      connection_container_.add_connection(std::move(new_connection));
      net::defer(io_ctx, [new_conn_ptr]() { new_conn_ptr->async_run(); });
    } break;
  }
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
      resolver.resolve(context_.get_bind_address().address(),
                       std::to_string(context_.get_bind_address().port()));

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
      log_warning("[%s] failed to open socket for %s: %s",
                  context_.get_name().c_str(),
                  mysqlrouter::to_string(addr.endpoint()).c_str(),
                  last_res.error().message().c_str());
      continue;
    }

    last_res = sock.set_option(net::socket_base::reuse_address{true});
    if (!last_res) {
      log_warning("[%s] failed to set reuse_address socket option for %s: %s",
                  context_.get_name().c_str(),
                  mysqlrouter::to_string(addr.endpoint()).c_str(),
                  last_res.error().message().c_str());
      continue;
    }

    last_res = sock.bind(addr.endpoint());
    if (!last_res) {
      log_warning("[%s] failed to bind(%s): %s", context_.get_name().c_str(),
                  mysqlrouter::to_string(addr.endpoint()).c_str(),
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
    //    std::string replicaset_name = kDefaultReplicaSetName;

    //    if (uri.path.size() > 0 && !uri.path[0].empty())
    //      replicaset_name = uri.path[0];

    destination_ = std::make_unique<DestMetadataCacheGroup>(
        io_ctx_, uri.host, routing_strategy_, uri.query,
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
    const Protocol::Type protocol) {
  switch (strategy) {
    case RoutingStrategy::kFirstAvailable:
      return std::make_unique<DestFirstAvailable>(io_ctx, protocol);
    case RoutingStrategy::kNextAvailable:
      return std::make_unique<DestNextAvailable>(io_ctx, protocol);
    case RoutingStrategy::kRoundRobin:
      return std::make_unique<DestRoundRobin>(io_ctx, protocol);
    case RoutingStrategy::kUndefined:
    case RoutingStrategy::kRoundRobinWithFallback:;  // unsupported, fall
                                                     // through
  }

  throw std::runtime_error("Wrong routing strategy " +
                           std::to_string(static_cast<int>(strategy)));
}
}  // namespace

void MySQLRouting::set_destinations_from_csv(const std::string &csv) {
  std::stringstream ss(csv);
  std::string part;

  // if no routing_strategy is defined for standalone routing
  // we set the default based on the mode
  if (routing_strategy_ == RoutingStrategy::kUndefined) {
    routing_strategy_ = get_default_routing_strategy(access_mode_);
  }

  is_destination_standalone_ = true;
  destination_ = create_standalone_destination(io_ctx_, routing_strategy_,
                                               context_.get_protocol());

  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    mysql_harness::trim(part);
    auto make_res = mysql_harness::make_tcp_address(part);
    if (!make_res) {
      throw std::runtime_error(
          string_format("Destination address '%s' is invalid", part.c_str()));
    }

    auto addr = make_res.value();

    if (mysql_harness::is_valid_domainname(addr.address())) {
      if (addr.port() == 0) {
        addr.port(Protocol::get_default_port(context_.get_protocol()));
      }

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
  if (maximum < 0 || maximum > static_cast<int>(UINT16_MAX)) {
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

bool MySQLRouting::is_accepting_connections() const {
  return acceptor_waitable_.serialize_with_cv([this](auto &, auto &) {
    if (service_tcp_.is_open()) {
      return true;
#if !defined(_WIN32)
    } else if (service_named_socket_.is_open()) {
      return true;
#endif
    }
    return false;
  });
}
