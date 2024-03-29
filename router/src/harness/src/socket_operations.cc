/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

using namespace std::string_literals;

namespace mysql_harness {

SocketOperations *SocketOperations::instance() {
  static SocketOperations instance_;
  return &instance_;
}

static void shrink(std::string &s) {
  // resize string to the first 0-char
  size_t nul_pos = s.find('\0');
  if (nul_pos != std::string::npos) {
    s.resize(nul_pos);
  }
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__FreeBSD__)
static stdx::expected<std::string, std::error_code> endpoint_to_name(
    const net::ip::tcp::endpoint &ep) {
  std::string buf;
  buf.resize(1024);

  const auto resolve_res = net::impl::resolver::getnameinfo(
      reinterpret_cast<const sockaddr *>(ep.data()), ep.size(), &buf.front(),
      buf.size(), nullptr, 0, NI_NAMEREQD);

  if (!resolve_res) {
    return resolve_res.get_unexpected();
  }

  shrink(buf);

  if (buf.empty()) {
    return stdx::make_unexpected(
        make_error_code(net::ip::resolver_errc::host_not_found));
  }

  return buf;
}
#endif

static SocketOperationsBase::LocalHostnameResolutionError
make_local_hostname_resolution_error(const std::error_code &ec) {
  return SocketOperationsBase::LocalHostnameResolutionError(
      "Could not get local host address: "s + ec.message() +
      "(errno: " + std::to_string(ec.value()) + ")");
}

std::string SocketOperations::get_local_hostname() {
#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__)
  std::string buf;
  buf.resize(1024);

  const auto res = net::impl::resolver::gethostname(&buf.front(), buf.size());
  if (!res) {
    throw make_local_hostname_resolution_error(res.error());
  }

  shrink(buf);

  if (buf.empty()) {
    throw make_local_hostname_resolution_error(
        make_error_code(net::ip::resolver_errc::host_not_found));
  }

  return buf;
#else
  net::NetworkInterfaceResolver netif_resolver;

  const auto netifs_res = netif_resolver.query();
  if (!netifs_res) {
    throw make_local_hostname_resolution_error(netifs_res.error());
  }
  const auto netifs = netifs_res.value();

  std::error_code last_ec{
      make_error_code(net::ip::resolver_errc::host_not_found)};

  for (auto const &netif : netifs) {
    // skip loopback interface
    if (netif.flags().value() & IFF_LOOPBACK) continue;
    // skip interfaces that aren't up
    if (!(netif.flags().value() & IFF_UP)) continue;

    for (auto const &net : netif.v6_networks()) {
      if (net.network().is_loopback()) continue;
      if (net.network().is_link_local()) continue;

      const auto ep = net::ip::tcp::endpoint(net.address(), 3306);

      auto name_res = endpoint_to_name(ep);
      if (!name_res) {
        last_ec = name_res.error();
        continue;
      }

      return name_res.value();
    }

    for (auto const &net : netif.v4_networks()) {
      if (net.network().is_loopback()) continue;

      const auto ep = net::ip::tcp::endpoint(net.address(), 3306);

      auto name_res = endpoint_to_name(ep);
      if (!name_res) {
        last_ec = name_res.error();
        continue;
      }

      return name_res.value();
    }
  }

  // - no interface found
  // - no non-loopback interface found
  // - interface with v4/v6 addresses found, but non have a name assigned.

  throw make_local_hostname_resolution_error(last_ec);
#endif
}

}  // namespace mysql_harness
