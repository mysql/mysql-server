/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "mysql/harness/sd_notify.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected.h"

#ifndef _WIN32
#include <sys/un.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <array>
#include <cstddef>
#include <cstring>

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/local.h"

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

static bool notify(const std::string &msg) {
  const std::string pipe_name = get_notify_socket_name();
  if (pipe_name.empty()) {
    log_debug("NOTIFY_SOCKET is empty, skipping sending '%s' notification",
              msg.c_str());
    return false;
  }

  log_debug("Using NOTIFY_SOCKET='%s' for the '%s' notification",
            pipe_name.c_str(), msg.c_str());

  DWORD written;
  HANDLE hPipe =
      CreateFile(TEXT(pipe_name.c_str()), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hPipe != INVALID_HANDLE_VALUE) {
    const auto res =
        WriteFile(hPipe, msg.c_str(), msg.length() + 1, &written, NULL);

    CloseHandle(hPipe);
    if (res) return true;
  }

  log_warning(
      "Failed to send notification '%s' to the named pipe '%s', error=%d",
      msg.c_str(), pipe_name.c_str(), GetLastError());

  return false;
}

#else

static stdx::expected<local::datagram_protocol::socket, std::error_code>
connect_to_notify_socket(net::io_context &io_ctx,
                         const std::string &socket_name) {
  if (socket_name.empty()) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  auto sock_name = socket_name;
  // transform abstract namespace socket
  if (sock_name[0] == '@') sock_name[0] = '\0';

  const local::datagram_protocol::endpoint ep(sock_name);

  if (ep.path() != sock_name) {
    // socket name was truncated
    return stdx::make_unexpected(make_error_code(std::errc::filename_too_long));
  }

  local::datagram_protocol::socket sock(io_ctx);
  do {
    const auto connect_res = sock.connect(ep);
    if (!connect_res) {
      if (connect_res.error() != make_error_code(std::errc::interrupted)) {
        return connect_res.get_unexpected();
      }

      // stay in the loop in case we got interrupted.
    } else {
#if defined(__SUNPRO_CC)
      // suncc needs a std::move(), while gcc complains about redundant
      // std::move().
      return std::move(sock);
#else
      return sock;
#endif
    }
  } while (true);
}

static bool notify(const std::string &msg) {
  const std::string socket_name = get_notify_socket_name();
  if (socket_name.empty()) {
    log_debug("NOTIFY_SOCKET is empty, skipping sending '%s' notification",
              msg.c_str());
    return false;
  }

  log_debug("Using NOTIFY_SOCKET='%s' for the '%s' notification",
            socket_name.c_str(), msg.c_str());

  net::io_context io_ctx;
  auto connect_res = connect_to_notify_socket(io_ctx, socket_name);
  if (!connect_res) {
    log_warning("Could not connect to the NOTIFY_SOCKET='%s': %s",
                socket_name.c_str(), connect_res.error().message().c_str());

    return false;
  }

  auto sock = std::move(connect_res.value());

  const auto write_res = net::write(sock, net::buffer(msg));
  if (!write_res) {
    log_warning("Failed writing '%s' to the NOTIFY_SOCKET='%s': %s",
                msg.c_str(), socket_name.c_str(),
                write_res.error().message().c_str());
    return false;
  }

  return true;
}

#endif

bool notify_ready() { return notify("READY=1"); }

bool notify_stopping() {
  return notify("STOPPING=1\nSTATUS=Router shutdown in progress\n");
}

}  // namespace mysql_harness
