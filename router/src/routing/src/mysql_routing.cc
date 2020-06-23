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

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <sys/types.h>

#if defined(__sun)
#include <ucred.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/ucred.h>
#endif

#include "common.h"  // rename_thread
#include "connection.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "dest_next_available.h"
#include "dest_round_robin.h"
#include "mysql/harness/filesystem.h"  // make_file_private
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/io/file_handle.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "plugin_config.h"
#include "protocol/protocol.h"
#include "socket_operations.h"
#include "tcp_address.h"

using mysqlrouter::string_format;
using routing::AccessMode;
using routing::RoutingStrategy;
IMPORT_LOG_FUNCTIONS()

static int kListenQueueSize = 1024;

static const char *kDefaultReplicaSetName = "default";
static const std::chrono::milliseconds kAcceptorStopPollInterval_ms{100};

MySQLRouting::MySQLRouting(
    net::io_context &io_ctx, routing::RoutingStrategy routing_strategy,
    uint16_t port, const Protocol::Type protocol,
    const routing::AccessMode access_mode, const string &bind_address,
    const mysql_harness::Path &named_socket, const string &route_name,
    int max_connections, std::chrono::milliseconds destination_connect_timeout,
    unsigned long long max_connect_errors,
    std::chrono::milliseconds client_connect_timeout,
    unsigned int net_buffer_length,
    mysql_harness::SocketOperationsBase *sock_ops, size_t thread_stack_size)
    : context_(Protocol::create(protocol, sock_ops), sock_ops, route_name,
               net_buffer_length, destination_connect_timeout,
               client_connect_timeout,
               mysql_harness::TCPAddress(bind_address, port), named_socket,
               max_connect_errors, thread_stack_size),
      sock_ops_(sock_ops),
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

  assert(sock_ops_ != nullptr);

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

#if !defined(_WIN32)
/*
 * get PID and UID of the other end of the unix-socket
 */
static int unix_getpeercred(int sock, pid_t &peer_pid, uid_t &peer_uid) {
#if defined(__sun)
  ucred_t *ucred{nullptr};

  if (getpeerucred(sock, &ucred) == -1) {
    return -1;
  }

  peer_pid = ucred_getpid(ucred);
  peer_uid = ucred_getruid(ucred);

  free(ucred);

  return 0;
#elif defined(_GNU_SOURCE)
  struct ucred ucred;
  socklen_t ucred_len = sizeof(ucred);

  if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_len) == -1) {
    return -1;
  }

  peer_pid = ucred.pid;
  peer_uid = ucred.uid;

  return 0;
#else
  // tag them as UNUSED to keep -Werror happy
  (void)(sock);
  (void)(peer_pid);
  (void)(peer_uid);

  return -1;
#endif
}
#endif

void MySQLRouting::start_acceptor(mysql_harness::PluginFuncEnv *env) {
  mysql_harness::rename_thread(
      get_routing_thread_name(context_.get_name(), "RtA")
          .c_str());  // "Rt Acceptor" would be too long :(

  destination_->start(env);
  auto socket_ops = context_.get_socket_operations();

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

  if (service_tcp_.is_open()) {
    service_tcp_.native_non_blocking(true);
  }
#if !defined(_WIN32)
  if (service_named_socket_.is_open()) {
    service_named_socket_.native_non_blocking(true);
  }
#endif

  std::array<struct pollfd, 2> fds = {{
      {routing::kInvalidSocket, POLLIN, 0},
      {routing::kInvalidSocket, POLLIN, 0},
  }};

  const int kAcceptTcpNdx = 0;
  fds[kAcceptTcpNdx].fd = service_tcp_.native_handle();
#if !defined(_WIN32)
  const int kAcceptUnixSocketNdx = 1;
  fds[kAcceptUnixSocketNdx].fd = service_named_socket_.native_handle();
#endif

  mysql_harness::on_service_ready(env);

  while (is_running(env)) {
    // wait for the accept() sockets to become readable (POLLIN)
    const auto poll_res =
        socket_ops->poll(fds.data(), fds.size(), kAcceptorStopPollInterval_ms);

    if (!poll_res) {
      if (poll_res.error() == make_error_condition(std::errc::interrupted) ||
          poll_res.error() ==
              make_error_condition(std::errc::operation_would_block) ||
          poll_res.error() == make_error_condition(std::errc::timed_out)) {
        continue;
      } else {
        log_error("[%s] poll() failed with error: %s",
                  context_.get_name().c_str(),
                  poll_res.error().message().c_str());
        // leave the loop
        break;
      }
    }

    auto ready_fdnum = poll_res.value();

    for (size_t ndx = 0; ndx < sizeof(fds) / sizeof(fds[0]) && ready_fdnum > 0;
         ndx++) {
      // walk through all fields and check which fired

      if ((fds[ndx].revents & POLLIN) == 0) {
        continue;
      }

      --ready_fdnum;

      struct sockaddr_storage client_addr;
      auto sin_size = static_cast<socklen_t>(sizeof client_addr);

      auto accept_res = net::impl::socket::accept(
          fds[ndx].fd, (struct sockaddr *)&client_addr, &sin_size);

      if (!accept_res) {
        log_error("[%s] Failed accepting connection: %s",
                  context_.get_name().c_str(),
                  accept_res.error().message().c_str());
        continue;
      }

      mysql_harness::socket_t sock_client = accept_res.value();

      bool is_tcp = (ndx == kAcceptTcpNdx);

      if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
        // if the messages wouldn't be logged, don't get the peercreds
        if (is_tcp) {
          log_debug("[%s] fd=%d connection accepted at %s",
                    context_.get_name().c_str(), sock_client,
                    context_.get_bind_address().str().c_str());
        } else {
#if !defined(_WIN32)
          pid_t peer_pid;
          uid_t peer_uid;

          // try to be helpful of who tried to connect to use and failed.
          // who == PID + UID
          //
          // if we can't get the PID, we'll just show a simpler errormsg

          if (0 == unix_getpeercred(sock_client, peer_pid, peer_uid)) {
            log_debug(
                "[%s] fd=%d connection accepted at %s from (pid=%d, uid=%d)",
                context_.get_name().c_str(), sock_client,
                context_.get_bind_named_socket().str().c_str(), peer_pid,
                peer_uid);
          } else
          // fall through
#endif
            log_debug("[%s] fd=%d connection accepted at %s",
                      context_.get_name().c_str(), sock_client,
                      context_.get_bind_named_socket().str().c_str());
        }
      }

      // TODO: creation of new element by [] is most-likely unneccessary
      if (context_.conn_error_counters_[in_addr_to_array(client_addr)] >=
          context_.max_connect_errors_) {
        std::string client_name, msg;
        try {
          client_name = get_peer_name(&client_addr, socket_ops).first;
        } catch (const std::runtime_error &err) {
          log_error("Failed retrieving client address: %s", err.what());
          client_name = "[unknown]";
        }
        msg = "Too many connection errors from " + client_name;
        context_.get_protocol().send_error(sock_client, 1129, msg, "HY000",
                                           context_.get_name());
        log_info("%s", msg.c_str());
        socket_ops->close(sock_client);  // no shutdown() before close()
        continue;
      }

      if (context_.info_active_routes_.load(std::memory_order_relaxed) >=
          max_connections_) {
        context_.get_protocol().send_error(
            sock_client, 1040, "Too many connections to MySQL Router", "08004",
            context_.get_name());
        socket_ops->close(sock_client);  // no shutdown() before close()
        log_warning("[%s] reached max active connections (%d max=%d)",
                    context_.get_name().c_str(),
                    context_.info_active_routes_.load(), max_connections_);
        continue;
      }

      if (is_tcp) {
        int opt_nodelay = 1;
        auto sockopt_res =
            net::impl::socket::setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY,
                                          &opt_nodelay, sizeof(opt_nodelay));
        if (!sockopt_res) {
          log_info("[%s] fd=%d client setsockopt(TCP_NODELAY) failed: %s",
                   context_.get_name().c_str(), sock_client,
                   sockopt_res.error().message().c_str());

          // if it fails, it will be slower, but cause no harm
        }
      }

      // On some OS'es the socket will be non-blocking as a result of accept()
      // on non-blocking socket. We need to make sure it's always blocking.
      socket_ops->set_socket_blocking(sock_client, true);

      // launch client thread which will service this new connection
      create_connection(sock_client, client_addr);
    }
  }  // while (is_running(env))

  // disconnect all connections
  connection_container_.disconnect_all();

  // wait until all connections are closed
  {
    std::unique_lock<std::mutex> lk(
        connection_container_.connection_removed_cond_m_);
    connection_container_.connection_removed_cond_.wait(
        lk, [&] { return connection_container_.empty(); });
  }

  log_info("[%s] stopped", context_.get_name().c_str());
}

static stdx::expected<net::impl::socket::native_handle_type, std::error_code>
get_server_socket(mysql_harness::SocketOperationsBase *so, Destination *dest,
                  std::chrono::milliseconds connect_timeout) {
  // resolve
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  const auto addrinfo_res = so->getaddrinfo(
      dest->hostname().c_str(), std::to_string(dest->port()).c_str(), &hints);

  std::error_code last_ec{};
  net::impl::socket::native_handle_type sock{net::impl::socket::kInvalidSocket};
  auto const *ai = addrinfo_res.value().get();
  for (; ai != nullptr; ai = ai->ai_next) {
    auto sock_type = ai->ai_socktype;
#if defined(__linux__) || defined(__FreeBSD__)
    // linux|freebsd allows to set NONBLOCK as part of the socket() call to safe
    // the extra syscall
    sock_type |= SOCK_NONBLOCK;
#endif
    auto socket_res = so->socket(ai->ai_family, sock_type, ai->ai_protocol);
    if (!socket_res) {
      continue;
    }

    sock = socket_res.value();

    so->set_socket_blocking(sock, false);

    auto connect_res = so->connect(sock, ai->ai_addr, ai->ai_addrlen);

    if (!connect_res) {
      if (connect_res.error() ==
              make_error_condition(std::errc::operation_in_progress) ||
          connect_res.error() ==
              make_error_condition(std::errc::operation_would_block)) {
        const auto wait_res =
            so->connect_non_blocking_wait(sock, connect_timeout);

        if (!wait_res) {
          log_warning(
              "Timeout reached trying to connect to MySQL Server %s: %s",
              dest->hostname().c_str(), wait_res.error().message().c_str());

          last_ec = wait_res.error();
        } else {
          const auto status_res = so->connect_non_blocking_status(sock);
          if (status_res) {
            // success, we can continue
            break;
          }

          last_ec = status_res.error();
        }
      } else {
        log_debug("Failed connect() to %s:%u: %s", dest->hostname().c_str(),
                  dest->port(), connect_res.error().message().c_str());

        last_ec = connect_res.error();
      }
    } else {
      // everything is fine, we are connected
      break;
    }

    // some error, close the socket again and try the next one
    so->close(sock);
    sock = net::impl::socket::kInvalidSocket;
  }

  if (nullptr == ai) {
    return stdx::make_unexpected(last_ec);
  }

  // set blocking; MySQL protocol is blocking and we do not take advantage of
  // any non-blocking possibilities
  so->set_socket_blocking(sock, true);

  int opt_nodelay = 1;
  const auto sockopt_res = so->setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                                          &opt_nodelay, sizeof opt_nodelay);
  if (!sockopt_res) {
    log_debug("Failed setting TCP_NODELAY on client socket: %s",
              sockopt_res.error().message().c_str());

    // log it, but otherwise ignore the error.
  }

  return sock;
}

void MySQLRouting::create_connection(int client_socket,
                                     const sockaddr_storage &client_addr) {
  auto remove_callback = [this](MySQLRoutingConnection *connection) {
    connection_container_.remove_connection(connection);
  };

  mysql_harness::TCPAddress server_address;
  mysql_harness::socket_t server_socket{mysql_harness::kInvalidSocket};

  {
    auto dests = destination_->destinations();
    do {
      for (auto const &dest : dests) {
        if (!dest->good()) continue;

        auto server_sock_res =
            get_server_socket(context_.get_socket_operations(), dest.get(),
                              context_.get_destination_connect_timeout());
        if (server_sock_res) {
          server_socket = server_sock_res.value();
          server_address =
              mysql_harness::TCPAddress(dest->hostname(), dest->port());

          break;
        }

        // report the connect status for this backend
        dest->connect_status(server_sock_res.error());
      }

      if (server_socket == mysql_harness::kInvalidSocket) {
        // no connection made to any of the destinations. Check if we can
        // refresh.
        auto refresh_res = destination_->refresh_destinations(dests);

        if (refresh_res) {
          dests = std::move(refresh_res.value());
        } else {
          break;
        }
      }

      // if no connection yet, but the refresh resulted in new
      // destinations -> retry.
    } while ((server_socket == mysql_harness::kInvalidSocket) &&
             !dests.empty());
  }

  auto new_connection = std::make_unique<MySQLRoutingConnection>(
      context_, client_socket, client_addr, server_socket, server_address,
      remove_callback);

  // - add connection to the container,
  // - start the connection thread
  //   - either starts a thread which calls remove_callback at end
  //   - or fails to start and calls remove_callback
  auto *new_conn_ptr = new_connection.get();

  connection_container_.add_connection(std::move(new_connection));
  new_conn_ptr->start();
}

// throws std::runtime_error
/*static*/ void MySQLRouting::set_unix_socket_permissions(
    const char *socket_file) {
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
    return last_res;
  }

  local::stream_protocol::endpoint ep(socket_file);

  last_res = sock.bind(ep);
  if (!last_res) {
    if (last_res.error() != make_error_code(std::errc::address_in_use)) {
      return last_res;
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
        return last_res;
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
  if (!last_res) return last_res;

  service_named_socket_ = std::move(sock);

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

    destination_.reset(new DestMetadataCacheGroup(
        uri.host, replicaset_name, routing_strategy_, uri.query,
        context_.get_protocol().get_type(), access_mode_));
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

RouteDestination *create_standalone_destination(
    const routing::RoutingStrategy strategy, const Protocol::Type protocol,
    mysql_harness::SocketOperationsBase *sock_ops, size_t thread_stack_size) {
  switch (strategy) {
    case RoutingStrategy::kFirstAvailable:
      return new DestFirstAvailable(protocol, sock_ops);
    case RoutingStrategy::kNextAvailable:
      return new DestNextAvailable(protocol, sock_ops);
    case RoutingStrategy::kRoundRobin:
      return new DestRoundRobin(protocol, sock_ops, thread_stack_size);
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

  destination_.reset(create_standalone_destination(
      routing_strategy_, context_.get_protocol().get_type(), sock_ops_,
      context_.get_thread_stack_size()));

  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    info = mysqlrouter::split_addr_port(part);
    if (info.second == 0) {
      info.second =
          Protocol::get_default_port(context_.get_protocol().get_type());
    }
    mysql_harness::TCPAddress addr(info.first, info.second);
    if (addr.is_valid()) {
      destination_->add(addr);
    } else {
      throw std::runtime_error(string_format(
          "Destination address '%s' is invalid", addr.str().c_str()));
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
