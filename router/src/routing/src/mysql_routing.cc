/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
#ifdef _WIN32
#define NOMINMAX
#endif

#include "mysql_routing.h"
#include "common.h"
#include "connection.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "dest_next_available.h"
#include "dest_round_robin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql_routing_common.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "plugin_config.h"
#include "protocol/protocol.h"
#include "utils.h"

#include "mysql_router_thread.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>

#include <sys/types.h>

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#if defined(__sun)
#include <ucred.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/ucred.h>
#endif

using mysql_harness::TCPAddress;
using mysql_harness::get_strerror;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::get_socket_errno;
using mysqlrouter::is_valid_socket_name;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;
using routing::RoutingStrategy;
using std::runtime_error;
using std::string;
IMPORT_LOG_FUNCTIONS()

static int kListenQueueSize = 1024;

static const char *kDefaultReplicaSetName = "default";
static const std::chrono::milliseconds kAcceptorStopPollInterval_ms{100};

MySQLRouting::MySQLRouting(
    routing::RoutingStrategy routing_strategy, uint16_t port,
    const Protocol::Type protocol, const routing::AccessMode access_mode,
    const string &bind_address, const mysql_harness::Path &named_socket,
    const string &route_name, int max_connections,
    std::chrono::milliseconds destination_connect_timeout,
    unsigned long long max_connect_errors,
    std::chrono::milliseconds client_connect_timeout,
    unsigned int net_buffer_length,
    routing::RoutingSockOpsInterface *routing_sock_ops,
    size_t thread_stack_size)
    : context_(Protocol::create(protocol, routing_sock_ops),
               routing_sock_ops->so(), route_name, net_buffer_length,
               destination_connect_timeout, client_connect_timeout,
               TCPAddress(bind_address, port), named_socket, max_connect_errors,
               thread_stack_size),
      routing_sock_ops_(routing_sock_ops),
      routing_strategy_(routing_strategy),
      access_mode_(access_mode),
      max_connections_(set_max_connections(max_connections)),
      service_tcp_(routing::kInvalidSocket),
      service_named_socket_(routing::kInvalidSocket) {
  validate_destination_connect_timeout(destination_connect_timeout);

  assert(routing_sock_ops_ != nullptr);

#ifdef _WIN32
  if (named_socket.is_set()) {
    throw std::invalid_argument(string_format(
        "'socket' configuration item is not supported on Windows platform"));
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

MySQLRouting::~MySQLRouting() {
  if (service_tcp_ != routing::kInvalidSocket) {
    context_.get_socket_operations()->shutdown(service_tcp_);
    context_.get_socket_operations()->close(service_tcp_);
  }
}

void MySQLRouting::start(mysql_harness::PluginFuncEnv *env) {
  mysql_harness::rename_thread(
      get_routing_thread_name(context_.get_name(), "RtM")
          .c_str());  // "Rt main" would be too long :(
  if (context_.get_bind_address().port > 0) {
    try {
      setup_tcp_service();
    } catch (const runtime_error &exc) {
      clear_running(env);
      throw runtime_error(
          string_format("Setting up TCP service using %s: %s",
                        context_.get_bind_address().str().c_str(), exc.what()));
    }
    log_info("[%s] started: listening on %s", context_.get_name().c_str(),
             context_.get_bind_address().str().c_str());
  }
#ifndef _WIN32
  if (context_.get_bind_named_socket().is_set()) {
    try {
      setup_named_socket_service();
    } catch (const runtime_error &exc) {
      clear_running(env);
      throw runtime_error(
          string_format("Setting up named socket service '%s': %s",
                        context_.get_bind_named_socket().c_str(), exc.what()));
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
      if (errno != ENOENT)
        log_warning("%s", ("Failed removing socket file " +
                           context_.get_bind_named_socket().str() + " (" +
                           get_strerror(errno) + " (" + to_string(errno) + "))")
                              .c_str());
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

  destination_->start();

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

  if (service_tcp_ != routing::kInvalidSocket) {
    routing::set_socket_blocking(service_tcp_, false);
  }
  if (service_named_socket_ != routing::kInvalidSocket) {
    routing::set_socket_blocking(service_named_socket_, false);
  }

  const int kAcceptUnixSocketNdx = 0;
  const int kAcceptTcpNdx = 1;
  struct pollfd fds[] = {
      {routing::kInvalidSocket, POLLIN, 0},
      {routing::kInvalidSocket, POLLIN, 0},
  };

  fds[kAcceptTcpNdx].fd = service_tcp_;
  fds[kAcceptUnixSocketNdx].fd = service_named_socket_;

  while (is_running(env)) {
    // wait for the accept() sockets to become readable (POLLIN)
    int ready_fdnum = context_.get_socket_operations()->poll(
        fds, sizeof(fds) / sizeof(fds[0]), kAcceptorStopPollInterval_ms);
    // < 0 - failure
    // == 0 - timeout
    // > 0  - number of pollfd's with a .revent

    if (ready_fdnum < 0) {
      const int last_errno = context_.get_socket_operations()->get_errno();
      switch (last_errno) {
        case EINTR:
        case EAGAIN:
          continue;
        default:
          log_error("[%s] poll() failed with error: %s",
                    context_.get_name().c_str(),
                    get_message_error(last_errno).c_str());
          break;
      }
    }

    for (size_t ndx = 0; ndx < sizeof(fds) / sizeof(fds[0]) && ready_fdnum > 0;
         ndx++) {
      // walk through all fields and check which fired

      if ((fds[ndx].revents & POLLIN) == 0) {
        continue;
      }

      --ready_fdnum;

      int sock_client;
      struct sockaddr_storage client_addr;
      socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);

      if ((sock_client = accept(fds[ndx].fd, (struct sockaddr *)&client_addr,
                                &sin_size)) < 0) {
        log_error(
            "[%s] Failed accepting connection: %s", context_.get_name().c_str(),
            get_message_error(context_.get_socket_operations()->get_errno())
                .c_str());
        continue;
      }

      bool is_tcp = (ndx == kAcceptTcpNdx);

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

      if (context_.conn_error_counters_[in_addr_to_array(client_addr)] >=
          context_.max_connect_errors_) {
        std::string client_name, msg;
        try {
          client_name =
              get_peer_name(&client_addr, context_.get_socket_operations())
                  .first;
        } catch (std::runtime_error &err) {
          log_error("Failed retrieving client address: %s", err.what());
          client_name = "[unknown]";
        }
        msg = "Too many connection errors from " + client_name;
        context_.get_protocol().send_error(sock_client, 1129, msg, "HY000",
                                           context_.get_name());
        log_info("%s", msg.c_str());
        context_.get_socket_operations()->close(
            sock_client);  // no shutdown() before close()
        continue;
      }

      if (context_.info_active_routes_.load(std::memory_order_relaxed) >=
          max_connections_) {
        context_.get_protocol().send_error(
            sock_client, 1040, "Too many connections to MySQL Router", "08004",
            context_.get_name());
        context_.get_socket_operations()->close(
            sock_client);  // no shutdown() before close()
        log_warning("[%s] reached max active connections (%d max=%d)",
                    context_.get_name().c_str(),
                    context_.info_active_routes_.load(), max_connections_);
        continue;
      }

      int opt_nodelay = 1;
      if (is_tcp && setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY,
                               reinterpret_cast<char *>(&opt_nodelay),
                               static_cast<socklen_t>(sizeof(int))) == -1) {
        log_info(
            "[%s] fd=%d client setsockopt(TCP_NODELAY) failed: %s",
            context_.get_name().c_str(), sock_client,
            get_message_error(context_.get_socket_operations()->get_errno())
                .c_str());

        // if it fails, it will be slower, but cause no harm
      }

      // On some OS'es the socket will be non-blocking as a result of accept()
      // on non-blocking socket. We need to make sure it's always blocking.
      routing::set_socket_blocking(sock_client, true);

      // launch client thread which will service this new connection
      create_connection(sock_client, client_addr);
    }
  }  // while (is_running(env))

  // disconnect all connections
  connection_container_.disconnect_all();

  // wait until all connections are closed
  {
    std::unique_lock<std::mutex> lk(context_.active_client_threads_cond_m_);
    context_.active_client_threads_cond_.wait(
        lk, [&] { return context_.active_client_threads_ == 0; });
  }

  log_info("[%s] stopped", context_.get_name().c_str());
}

void MySQLRouting::create_connection(int client_socket,
                                     const sockaddr_storage &client_addr) {
  auto remove_callback = [this](MySQLRoutingConnection *connection) {
    connection_container_.remove_connection(connection);
  };

  int error = 0;
  mysql_harness::TCPAddress server_address;
  int server_socket = destination_->get_server_socket(
      context_.get_destination_connect_timeout(), &error, &server_address);

  std::unique_ptr<MySQLRoutingConnection> new_connection(
      new MySQLRoutingConnection(context_, client_socket, client_addr,
                                 server_socket, server_address,
                                 remove_callback));

  new_connection->start();
  connection_container_.add_connection(std::move(new_connection));
}

// throws std::runtime_error
/*static*/ void MySQLRouting::set_unix_socket_permissions(
    const char *socket_file) {
#ifndef _WIN32  // Windows doesn't have Unix sockets

  // make sure the socket is accessible to all users
  // NOTE: According to man 7 unix, only r+w access is required to connect to
  // socket, and indeed
  //       setting permissions to rw-rw-rw- seems to work just fine on
  //       Ubuntu 14.04. However, for some reason bind() creates rwxr-xr-x by
  //       default on said system, and Server 5.7 uses rwxrwxrwx for its socket
  //       files. To be compliant with Server, we make our permissions rwxrwxrwx
  //       as well, but the x is probably not necessary.
  bool failed = chmod(socket_file, S_IRUSR | S_IRGRP | S_IROTH |      // read
                                       S_IWUSR | S_IWGRP | S_IWOTH |  // write
                                       S_IXUSR | S_IXGRP | S_IXOTH);  // execute
  if (failed) {
    std::string msg =
        std::string("Failed setting file permissions on socket file '") +
        socket_file + "': " + get_strerror(errno);
    log_error("%s", msg.c_str());
    throw std::runtime_error(msg);
  }
#endif
}

void MySQLRouting::setup_tcp_service() {
  struct addrinfo *servinfo, *info, hints;
  int err;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  err = context_.get_socket_operations()->getaddrinfo(
      context_.get_bind_address().addr.c_str(),
      to_string(context_.get_bind_address().port).c_str(), &hints, &servinfo);
  if (err != 0) {
    throw runtime_error(
        string_format("[%s] Failed getting address information (%s)",
                      context_.get_name().c_str(), gai_strerror(err)));
  }

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    if (servinfo) context_.get_socket_operations()->freeaddrinfo(servinfo);
  });

  // Try to setup socket and bind
  std::string error;
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((service_tcp_ = context_.get_socket_operations()->socket(
             info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from socket(): %s",
                  context_.get_name().c_str(), error.c_str());
      continue;
    }

#if 1
    int option_value = 1;
    if (context_.get_socket_operations()->setsockopt(
            service_tcp_, SOL_SOCKET, SO_REUSEADDR, &option_value,
            static_cast<socklen_t>(sizeof(int))) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from setsockopt(): %s",
                  context_.get_name().c_str(), error.c_str());
      context_.get_socket_operations()->close(service_tcp_);
      service_tcp_ = routing::kInvalidSocket;
      continue;
    }
#endif

    if (context_.get_socket_operations()->bind(service_tcp_, info->ai_addr,
                                               info->ai_addrlen) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from bind(): %s",
                  context_.get_name().c_str(), error.c_str());
      context_.get_socket_operations()->close(service_tcp_);
      service_tcp_ = routing::kInvalidSocket;
      continue;
    }

    break;
  }

  if (info == nullptr) {
    throw runtime_error(string_format("[%s] Failed to setup service socket: %s",
                                      context_.get_name().c_str(),
                                      error.c_str()));
  }

  if (context_.get_socket_operations()->listen(service_tcp_, kListenQueueSize) <
      0) {
    throw runtime_error(string_format(
        "[%s] Failed to start listening for connections using TCP",
        context_.get_name().c_str()));
  }
}

#ifndef _WIN32
void MySQLRouting::setup_named_socket_service() {
  struct sockaddr_un sock_unix;
  string socket_file = context_.get_bind_named_socket().str();
  errno = 0;

  assert(!socket_file.empty());

  std::string error_msg;
  if (!is_valid_socket_name(socket_file, error_msg)) {
    throw std::runtime_error(error_msg);
  }

  if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::invalid_argument(get_strerror(errno));
  }

  sock_unix.sun_family = AF_UNIX;
  std::strncpy(sock_unix.sun_path, socket_file.c_str(), socket_file.size() + 1);

retry:
  if (::bind(service_named_socket_, (struct sockaddr *)&sock_unix,
             static_cast<socklen_t>(sizeof(sock_unix))) == -1) {
    int save_errno = errno;
    if (errno == EADDRINUSE) {
      // file exists, try to connect to it to see if the socket is already in
      // use
      if (::connect(service_named_socket_, (struct sockaddr *)&sock_unix,
                    static_cast<socklen_t>(sizeof(sock_unix))) == 0) {
        log_error("Socket file %s already in use by another process",
                  socket_file.c_str());
        throw std::runtime_error("Socket file already in use");
      } else {
        if (errno == ECONNREFUSED) {
          log_warning(
              "Socket file %s already exists, but seems to be unused. Deleting "
              "and retrying...",
              socket_file.c_str());
          if (unlink(socket_file.c_str()) == -1) {
            if (errno != ENOENT) {
              log_warning("%s",
                          ("Failed removing socket file " + socket_file + " (" +
                           get_strerror(errno) + " (" + to_string(errno) + "))")
                              .c_str());
              throw std::runtime_error(
                  "Failed removing socket file " + socket_file + " (" +
                  get_strerror(errno) + " (" + to_string(errno) + "))");
            }
          }
          errno = 0;
          context_.get_socket_operations()->close(service_named_socket_);
          if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error(get_strerror(errno));
          }
          goto retry;
        } else {
          errno = save_errno;
        }
      }
    }
    log_error("Error binding to socket file %s: %s", socket_file.c_str(),
              get_strerror(errno).c_str());
    throw std::runtime_error(get_strerror(errno));
  }

  set_unix_socket_permissions(
      socket_file.c_str());  // throws std::runtime_error

  if (listen(service_named_socket_, kListenQueueSize) < 0) {
    throw runtime_error(
        "Failed to start listening for connections using named socket");
  }
}
#endif

void MySQLRouting::set_destinations_from_uri(const URI &uri) {
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
    throw runtime_error(string_format(
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
    routing::RoutingSockOpsInterface *routing_sock_ops,
    size_t thread_stack_size) {
  switch (strategy) {
    case RoutingStrategy::kFirstAvailable:
      return new DestFirstAvailable(protocol, routing_sock_ops);
    case RoutingStrategy::kNextAvailable:
      return new DestNextAvailable(protocol, routing_sock_ops);
    case RoutingStrategy::kRoundRobin:
      return new DestRoundRobin(protocol, routing_sock_ops, thread_stack_size);
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
      routing_strategy_, context_.get_protocol().get_type(), routing_sock_ops_,
      context_.get_thread_stack_size()));

  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    info = mysqlrouter::split_addr_port(part);
    if (info.second == 0) {
      info.second =
          Protocol::get_default_port(context_.get_protocol().get_type());
    }
    TCPAddress addr(info.first, info.second);
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
    std::string error_msg(
        "[" + context_.get_name() +
        "] tried to set destination_connect_timeout using invalid value, was " +
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
