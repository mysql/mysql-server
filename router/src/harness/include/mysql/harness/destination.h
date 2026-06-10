/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DESTINATION_INCLUDED
#define MYSQL_HARNESS_DESTINATION_INCLUDED

#include "harness_export.h"

#include <cstdint>
#include <string>
#include <system_error>
#include <variant>

#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

class HARNESS_EXPORT TcpDestination {
 public:
  TcpDestination() = default;

  TcpDestination(std::string hostname, uint16_t port)
      : hostname_(std::move(hostname)), port_(port) {}

  TcpDestination(const TcpDestination &) = default;
  TcpDestination(TcpDestination &&) = default;

  TcpDestination &operator=(const TcpDestination &) = default;
  TcpDestination &operator=(TcpDestination &&) = default;

  ~TcpDestination() = default;

  auto operator<=>(const TcpDestination &) const = default;

  const std::string &hostname() const { return hostname_; }
  void hostname(const std::string &hn) { hostname_ = hn; }

  uint16_t port() const { return port_; }
  void port(uint16_t prt) { port_ = prt; }

  std::string str() const;

 private:
  std::string hostname_;
  uint16_t port_{};
};

class HARNESS_EXPORT LocalDestination {
 public:
  LocalDestination() = default;

  LocalDestination(std::string path) : path_(std::move(path)) {}

  LocalDestination(const LocalDestination &) = default;
  LocalDestination(LocalDestination &&) = default;

  LocalDestination &operator=(const LocalDestination &) = default;
  LocalDestination &operator=(LocalDestination &&) = default;

  ~LocalDestination() = default;

  auto operator<=>(const LocalDestination &) const = default;

  std::string path() const { return path_; }
  void path(const std::string &pa) { path_ = pa; }

  std::string str() const;

 private:
  std::string path_;
};

class HARNESS_EXPORT Destination {
 public:
  Destination(TcpDestination dest) : dest_(std::move(dest)) {}
  Destination(LocalDestination dest) : dest_(std::move(dest)) {}

  auto operator<=>(const Destination &) const = default;

  bool is_tcp() const { return std::holds_alternative<TcpDestination>(dest_); }
  bool is_local() const { return !is_tcp(); }

  std::string str() const;

  TcpDestination &as_tcp() { return std::get<TcpDestination>(dest_); }
  const TcpDestination &as_tcp() const {
    return std::get<TcpDestination>(dest_);
  }

  LocalDestination &as_local() { return std::get<LocalDestination>(dest_); }
  const LocalDestination &as_local() const {
    return std::get<LocalDestination>(dest_);
  }

 private:
  std::variant<TcpDestination, LocalDestination> dest_;
};

/**
 * create a TcpDestination from a string.
 *
 * - ipv4
 * - ipv6
 * - hostname
 *
 * followed by optional port.
 *
 * If IPv6 is followed by port, the address port is expected to be wrapped in
 * '[]'
 */
stdx::expected<TcpDestination, std::error_code> HARNESS_EXPORT
make_tcp_destination(std::string dest);

}  // namespace mysql_harness

#endif
