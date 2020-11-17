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
#include "socket_operations.h"

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

static socket_t connect_to_notify_socket(const std::string &socket_name,
                                         std::string &out_error) {
  const size_t sunpathlen = sizeof(sockaddr_un::sun_path) - 1;
  if (socket_name.length() > sunpathlen || socket_name.empty()) {
    out_error = "Socket name " + socket_name +
                " is invalid: " + std::to_string(socket_name.length());
    return kInvalidSocket;
  }
  socket_t result = socket(AF_UNIX, SOCK_DGRAM, 0);

  sockaddr_un addr{};
  socklen_t addrlen;
  addr.sun_family = AF_UNIX;

  strcpy(addr.sun_path, socket_name.c_str());
  addrlen = offsetof(struct sockaddr_un, sun_path) +
            static_cast<socklen_t>(socket_name.length());
  if (socket_name[0] == '@') {
    // Abstract namespace socket
    addr.sun_path[0] = '\0';
  }

  int ret = -1;
  do {
    ret = connect(result, reinterpret_cast<const sockaddr *>(&addr), addrlen);
  } while (ret == -1 && errno == EINTR);
  if (ret == -1) {
    out_error = mysql_harness::get_strerror(errno);
    return kInvalidSocket;
  }

  return result;
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

  std::string connect_err;
  auto notify_socket = connect_to_notify_socket(socket_name, connect_err);
  if (notify_socket == kInvalidSocket) {
    log_warning("Could not connect to the NOTIFY_SOCKET='%s': %s",
                socket_name.c_str(), connect_err.c_str());
    return false;
  }

  auto *sock_ops = mysql_harness::SocketOperations::instance();

  std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { sock_ops->close(notify_socket); });

  auto write_res = sock_ops->write_all(notify_socket, msg.data(), msg.length());
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
