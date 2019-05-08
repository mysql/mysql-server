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

#include "mysqlrouter/routing.h"
#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils.h"
#include "router_config.h"
#include "utils.h"

#include <climits>
#include <cstring>

#ifndef _WIN32
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using mysql_harness::TCPAddress;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
IMPORT_LOG_FUNCTIONS()

namespace routing {

const int kDefaultWaitTimeout = 0;  // 0 = no timeout used
const int kDefaultMaxConnections = 512;
const std::chrono::seconds kDefaultDestinationConnectionTimeout{1};
const std::string kDefaultBindAddress = "127.0.0.1";
const unsigned int kDefaultNetBufferLength =
    16384;  // Default defined in latest MySQL Server
const unsigned long long kDefaultMaxConnectErrors =
    100;  // Similar to MySQL Server
const std::chrono::seconds kDefaultClientConnectTimeout{
    9};  // Default connect_timeout MySQL Server minus 1

// unused constant
// const int kMaxConnectTimeout = INT_MAX / 1000;

// keep in-sync with enum AccessMode
const std::vector<const char *> kAccessModeNames{nullptr, "read-write",
                                                 "read-only"};

AccessMode get_access_mode(const std::string &value) {
  for (unsigned int i = 1; i < kAccessModeNames.size(); ++i)
    if (strcmp(kAccessModeNames[i], value.c_str()) == 0)
      return static_cast<AccessMode>(i);
  return AccessMode::kUndefined;
}

std::string get_access_mode_names() {
  // +1 to skip undefined
  return mysql_harness::serial_comma(kAccessModeNames.begin() + 1,
                                     kAccessModeNames.end());
}

std::string get_access_mode_name(AccessMode access_mode) noexcept {
  if (access_mode == AccessMode::kUndefined)
    return "<not-set>";
  else
    return kAccessModeNames[static_cast<int>(access_mode)];
}

// keep in-sync with enum RoutingStrategy
const std::vector<const char *> kRoutingStrategyNames{
    nullptr, "first-available", "next-available", "round-robin",
    "round-robin-with-fallback"};

RoutingStrategy get_routing_strategy(const std::string &value) {
  for (unsigned int i = 1; i < kRoutingStrategyNames.size(); ++i)
    if (strcmp(kRoutingStrategyNames[i], value.c_str()) == 0)
      return static_cast<RoutingStrategy>(i);
  return RoutingStrategy::kUndefined;
}

std::string get_routing_strategy_names(bool metadata_cache) {
  // round-robin-with-fallback is not supported for static routing
  const std::vector<const char *> kRoutingStrategyNamesStatic{
      "first-available", "next-available", "round-robin"};

  // next-available is not supported for metadata-cache routing
  const std::vector<const char *> kRoutingStrategyNamesMetadataCache{
      "first-available", "round-robin", "round-robin-with-fallback"};

  const auto &v = metadata_cache ? kRoutingStrategyNamesMetadataCache
                                 : kRoutingStrategyNamesStatic;
  return mysql_harness::serial_comma(v.begin(), v.end());
}

std::string get_routing_strategy_name(
    RoutingStrategy routing_strategy) noexcept {
  if (routing_strategy == RoutingStrategy::kUndefined)
    return "<not set>";
  else
    return kRoutingStrategyNames[static_cast<int>(routing_strategy)];
}

void set_socket_blocking(int sock, bool blocking) {
  assert(!(sock < 0));
#ifndef _WIN32
  auto flags = fcntl(sock, F_GETFL, nullptr);
  assert(flags >= 0);
  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  fcntl(sock, F_SETFL, flags);
#else
  u_long mode = blocking ? 0 : 1;
  ioctlsocket(sock, FIONBIO, &mode);
#endif
}

RoutingSockOps *RoutingSockOps::instance(
    mysql_harness::SocketOperationsBase *sock_ops) {
  static RoutingSockOps routing_sock_ops(sock_ops);
  return &routing_sock_ops;
}

int RoutingSockOps::get_mysql_socket(
    mysql_harness::TCPAddress addr,
    std::chrono::milliseconds connect_timeout_ms, bool log) noexcept {
  struct addrinfo *servinfo, *info, hints;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  bool timeout_expired = false;

  int err;
  if ((err = ::getaddrinfo(addr.addr.c_str(), to_string(addr.port).c_str(),
                           &hints, &servinfo)) != 0) {
    if (log) {
#ifndef _WIN32
      std::string errstr{(err == EAI_SYSTEM)
                             ? get_message_error(so_->get_errno())
                             : gai_strerror(err)};
#else
      std::string errstr = get_message_error(err);
#endif
      log_debug("Failed getting address information for '%s' (%s)",
                addr.addr.c_str(), errstr.c_str());
    }
    return -1;
  }

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    if (servinfo) freeaddrinfo(servinfo);
  });

  int sock = routing::kInvalidSocket;

  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((sock = ::socket(info->ai_family, info->ai_socktype,
                         info->ai_protocol)) == -1) {
      log_error("Failed opening socket: %s",
                get_message_error(so_->get_errno()).c_str());
    } else {
      bool connection_is_good = true;

      set_socket_blocking(sock, false);

      if (::connect(sock, info->ai_addr, info->ai_addrlen) < 0) {
        switch (so_->get_errno()) {
#ifdef _WIN32
          case WSAEINPROGRESS:
          case WSAEWOULDBLOCK:
#else
          case EINPROGRESS:
#endif
            if (0 != so_->connect_non_blocking_wait(sock, connect_timeout_ms)) {
              log_warning(
                  "Timeout reached trying to connect to MySQL Server %s: %s",
                  addr.str().c_str(),
                  get_message_error(so_->get_errno()).c_str());
              connection_is_good = false;
              timeout_expired = (so_->get_errno() == ETIMEDOUT);
              break;
            }

            {
              int so_error = 0;
              if (0 != so_->connect_non_blocking_status(sock, so_error)) {
                connection_is_good = false;
                break;
              }
            }

            // success, we can continue
            break;
          default:
            log_debug("Failed connect() to %s: %s", addr.str().c_str(),
                      get_message_error(so_->get_errno()).c_str());
            connection_is_good = false;
            break;
        }
      } else {
        // everything is fine, we are connected
      }

      if (connection_is_good) {
        break;
      }

      // some error, close the socket again and try the next one
      so_->close(sock);
    }
  }

  if (info == nullptr) {
    // all connects failed.
    return timeout_expired ? -2 : -1;
  }

  // set blocking; MySQL protocol is blocking and we do not take advantage of
  // any non-blocking possibilities
  set_socket_blocking(sock, true);

  int opt_nodelay = 1;
  if (setsockopt(
          sock, IPPROTO_TCP, TCP_NODELAY,
          reinterpret_cast<const char *>(
              &opt_nodelay),  // cast keeps Windows happy (const void* on Unix)
          static_cast<socklen_t>(sizeof(int))) == -1) {
    log_debug("Failed setting TCP_NODELAY on client socket");
    so_->close(sock);

    return -1;
  }

  return sock;
}

}  // namespace routing
