/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/sd_notify.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <cstdlib>  // getenv

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/win32_named_pipe.h"
#include "mysql/harness/stdx/expected.h"

IMPORT_LOG_FUNCTIONS()

namespace mysql_harness {

// returns empty string if not set or failed to read
static std::string get_notify_socket_name() {
  constexpr const char *socket_env = "NOTIFY_SOCKET";
#ifdef _WIN32
  std::string env_str;
  size_t len = 0;
  env_str.resize(2000);

  if (getenv_s(&len, &env_str.front(), env_str.size(), socket_env) != 0) {
    return "";
  }
  if (len == 0) return "";
  env_str.resize(len - 1);
  return env_str;
#else
  const auto result = getenv(socket_env);
  return result ? result : "";
#endif
}

#ifdef _WIN32

static stdx::expected<void, std::error_code> notify(
    const std::string &msg, const std::string &pipe_name) {
  net::io_context io_ctx;
  local::byte_protocol::socket sock(io_ctx);

  auto connect_res = sock.connect({pipe_name});
  if (!connect_res) {
    return stdx::unexpected(connect_res.error());
  }
  auto write_res = net::write(sock, net::buffer(msg));
  if (!write_res) {
    return stdx::unexpected(write_res.error());
  }

  return {};
}

#else

static stdx::expected<local::datagram_protocol::socket, std::error_code>
connect_to_notify_socket(net::io_context &io_ctx,
                         const std::string &socket_name) {
  if (socket_name.empty()) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  auto sock_name = socket_name;
  // transform abstract namespace socket
  if (sock_name[0] == '@') sock_name[0] = '\0';

  const local::datagram_protocol::endpoint ep(sock_name);

  if (ep.path() != sock_name) {
    // socket name was truncated
    return stdx::unexpected(make_error_code(std::errc::filename_too_long));
  }

  local::datagram_protocol::socket sock(io_ctx);
  do {
    const auto connect_res = sock.connect(ep);
    if (!connect_res) {
      if (connect_res.error() != make_error_code(std::errc::interrupted)) {
        return stdx::unexpected(connect_res.error());
      }

      // stay in the loop in case we got interrupted.
    } else {
      return sock;
    }
  } while (true);
}

static stdx::expected<void, std::error_code> notify(
    const std::string &msg, const std::string &socket_name) {
  net::io_context io_ctx;
  auto connect_res = connect_to_notify_socket(io_ctx, socket_name);
  if (!connect_res) {
    return stdx::unexpected(connect_res.error());
  }

  auto sock = std::move(connect_res.value());

  const auto write_res = net::write(sock, net::buffer(msg));
  if (!write_res) {
    return stdx::unexpected(write_res.error());
  }

  return {};
}
#endif
static bool notify(const std::string &msg) {
  const std::string socket_name = get_notify_socket_name();
  if (socket_name.empty()) {
    log_debug("NOTIFY_SOCKET is empty, skipping sending '%s' notification",
              msg.c_str());
    return false;
  }

  log_debug("Using NOTIFY_SOCKET='%s' for the '%s' notification",
            socket_name.c_str(), msg.c_str());

  auto notify_res = notify(msg, socket_name);
  if (!notify_res) {
    log_warning("sending '%s' to NOTIFY_SOCKET='%s' failed: %s", msg.c_str(),
                socket_name.c_str(), notify_res.error().message().c_str());
    return false;
  }

  return true;
}

bool notify_status(const std::string &msg) { return notify("STATUS=" + msg); }

bool notify_ready() { return notify("READY=1"); }

bool notify_stopping() {
  return notify("STOPPING=1\nSTATUS=Router shutdown in progress\n");
}

}  // namespace mysql_harness
