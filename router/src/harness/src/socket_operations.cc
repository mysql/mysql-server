/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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

#include "socket_operations.h"

#include <array>

#include "mysql/harness/net_ts/impl/netif.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

SocketOperations *SocketOperations::instance() {
  static SocketOperations instance_;
  return &instance_;
}

std::string SocketOperations::get_local_hostname() {
  std::string buf;
  buf.resize(1024);

#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__)
  const auto res = net::impl::resolver::gethostname(&buf.front(), buf.size());
  if (!res) {
    throw LocalHostnameResolutionError(
        "Could not get local hostname: " + res.error().message() +
        " (error: " + std::to_string(res.error().value()) + ")");
  }

  // resize string to the first 0-char
  size_t nul_pos = buf.find('\0');
  if (nul_pos != std::string::npos) {
    buf.resize(nul_pos);
  }
#else
  net::NetworkInterfaceResolver netif_resolver;

  const auto netifs_res = netif_resolver.query();
  if (!netifs_res) {
    const auto ec = netifs_res.error();
    throw LocalHostnameResolutionError(
        "Could not get local host address: " + ec.message() +
        "(errno: " + std::to_string(ec.value()) + ")");
  }
  const auto netifs = netifs_res.value();

  std::error_code last_ec{};

  for (auto const &netif : netifs) {
    // skip loopback interface
    if (netif.flags().value() & IFF_LOOPBACK) continue;
    // skip interfaces that aren't up
    if (!(netif.flags().value() & IFF_UP)) continue;

    for (auto const &net : netif.v6_networks()) {
      if (net.network().is_loopback()) continue;
      if (net.network().is_link_local()) continue;

      const auto ep = net::ip::tcp::endpoint(net.address(), 3306);

      const auto resolve_res = net::impl::resolver::getnameinfo(
          reinterpret_cast<const sockaddr *>(ep.data()), ep.size(),
          &buf.front(), buf.size(), nullptr, 0, NI_NAMEREQD);

      if (!resolve_res) {
        last_ec = resolve_res.error();
        continue;
      }

      // resize string to the first 0-char
      size_t nul_pos = buf.find('\0');
      if (nul_pos != std::string::npos) {
        buf.resize(nul_pos);
      }

      if (!buf.empty()) {
        return buf;
      }
    }

    for (auto const &net : netif.v4_networks()) {
      if (net.network().is_loopback()) continue;

      const auto ep = net::ip::tcp::endpoint(net.address(), 3306);

      const auto resolve_res = net::impl::resolver::getnameinfo(
          reinterpret_cast<const sockaddr *>(ep.data()), ep.size(),
          &buf.front(), buf.size(), nullptr, 0, NI_NAMEREQD);

      if (!resolve_res) {
        last_ec = resolve_res.error();
        continue;
      }

      // resize string to the first 0-char
      size_t nul_pos = buf.find('\0');
      if (nul_pos != std::string::npos) {
        buf.resize(nul_pos);
      }

      if (!buf.empty()) {
        return buf;
      }
    }
  }

  if (last_ec &&
      (last_ec != make_error_code(net::ip::resolver_errc::host_not_found))) {
    throw LocalHostnameResolutionError(
        "Could not get local hostname: " + last_ec.message() +
        " (ret: " + std::to_string(last_ec.value()) + ")");
  }
#endif

  return buf;
}

}  // namespace mysql_harness
